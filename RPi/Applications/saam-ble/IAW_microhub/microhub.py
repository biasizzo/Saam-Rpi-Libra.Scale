if __name__ == "__main__":
    import os, sys

    sys.path.append(
        os.path.abspath(os.path.join(os.path.dirname(os.path.realpath(__file__)), ".."))
    )

from COMMON_abst.device import Device, DeviceQueue
from COMMON_abst.logger import get_logger as saam_get_log

from IAW_microhub.microhub_time_align import TimeAlign
import IAW_microhub.constants as constants
from IAW_microhub.constants import MicroHubType

import time

from func_timeout import func_timeout, FunctionTimedOut

from threading import Lock

# see protocol definitions
_ACCELEROMETER_G = [2, 4, 8, 16]
_GYRO_DPS = [250, 500, 1000, 2000]
_SENSOR_PERIOD = [225, 112.5, 56.25, 0]
_DMP = [0, 1]


class Microhub(Device):
    @staticmethod
    def getDeviceType():
        return "microhub"

    def __init__(
        self,
        type: MicroHubType,
        deviceMac: str,
        outputQueue: DeviceQueue = None,
        iface=None,
        name=None,
    ):

        if not type:
            raise Exception(
                "MicroHub type not known! (BED, WRIST, CLIP are possible options!)"
            )

        """Initialize MicroHub to default values."""
        dev_log_name = "MicroHub_{}".format(name if name else deviceMac)

        # set up logger
        logger = saam_get_log(dev_log_name, 1)
        Device.__init__(
            self,
            dev_log_name,
            "microhub",
            deviceMac,
            outputQueue,
            iface=iface,
            logger=logger,
        )

        self._type = type
        self._MISSING_DATA = float("nan")

        # current state of connected device
        self._curr_battery = -1
        self._curr_acc_g = -1
        self._curr_gyro_dps = -1
        self._curr_sensor_frequency = -1
        self._curr_dmp = -1

        # ble interface
        self._chSerialPortw = None
        self._chSerialPortr = None

        self._lastCounter = -1

        self._time_align = None

        self._init_done = False

        self._last_M3_packet_received = -1

        # see TR 2020_09_07
        self._wrist_first_packet_broken_workaround_done = 0

        self._ok_mutex = Lock()
        self._ok_received = False

    @property
    def microHubType(self) -> MicroHubType:
        return self._type

    @property
    def battery_level(self) -> int:
        return self._curr_battery

    @property
    def sampling_rate(self) -> float:
        return self._curr_sensor_frequency

    @property
    def gyro_dps(self) -> int:
        return self._curr_gyro_dps

    @property
    def accel_g(self) -> int:
        return self._curr_acc_g

    def _writeToSerial(self, data: bytes):
        # clear notifications so we do not interrupt whatever is happening on hci level
        while self.peripheral.waitForNotifications(0.001):
            continue
        self._chSerialPortw.write(data, True)  # with response

    def _writeAndWait(self, msg, wait=1.0, checkOk=False):
        self._log(" ! BLE command: {}".format(str(msg)))
        self.isOkReceived()  # clear "ok" state before this command
        self._writeToSerial(msg)
        time_s1 = time.time()
        while (time.time() - time_s1) < wait:
            self.peripheral.waitForNotifications(0.05)

        if checkOk:
            return self.waitForOkReceived(5)
        return False

    def _decodePacketCounter(self, data):
        return data[0] + (data[1] * 256)

    def _decodeBatteryPercentage(self, percent):
        if (
            (self.battery_level < 0)
            or (percent < self.battery_level)
            or (percent > (self.battery_level + 2))
        ):
            self._curr_battery = percent
            self._log("Battery percentage changed: {}".format(self.battery_level))
            if self.microHubType != MicroHubType.BED:  # not relevant for bed sensor
                self._sendToOutputDev({"battery": self.battery_level})

    def _decodeMetadata(self, mdata):
        a_fsr, g_dps, freq, dmp, e_bit = -1, -1, -1, -1, -1
        a_fsr = _ACCELEROMETER_G[mdata & 0x03]
        g_dps = _GYRO_DPS[(mdata >> 2) & 0x03]
        freq = _SENSOR_PERIOD[(mdata >> 4) & 0x03]
        dmp = _DMP[(mdata >> 6) & 0x01]
        # e_bit = (mdata >> 7) & 0x01

        if self.accel_g != a_fsr:
            self._curr_acc_g = a_fsr
            self._log("Accelerometer scale changed: {}".format(a_fsr))

        if self.gyro_dps != g_dps:
            self._curr_gyro_dps = g_dps
            self._log("Gyro scale changed: {}".format(g_dps))

        if self.sampling_rate != freq:
            self._curr_sensor_frequency = freq
            self._log("Sampling freq changed: {}".format(freq))

        if self._curr_dmp != dmp:
            self._curr_dmp = dmp
            self._log("DMP changed: {}".format(dmp))

        if not constants.checkConfig(
            self.microHubType, self.sampling_rate, self.accel_g, self.gyro_dps
        ):
            self.disconnect_ble()

    def _decode10bitsamples(self, samples):
        # this is a simple parser, since we expect 10 bit values in 2 bytes
        numbers = []
        for i in range(12):
            start_bit = i * 10
            lb = int(start_bit / 8)  # get start byte
            shift = start_bit % 8  # get num of places to shift to 0 bit
            low = samples[lb] >> shift
            high = samples[lb + 1] << (8 - shift)
            value = (low + high) & 0x1FF  # disregard sign bit
            if ((low + high) & 0x200) > 0:  # check for signed values
                value = -value
            numbers.append(value)
        return numbers

    def _parse4AccelSamples(self, data):
        # byte 0 is mode ID
        # bytes 1 and 2 are packet counter
        counter = self._decodePacketCounter(data[1:3])
        # byte 3 is battery %
        self._decodeBatteryPercentage(data[3])
        # byte 4 is metadata
        self._decodeMetadata(data[4])
        # till end of packet are 10 bit samples
        samples = self._decode10bitsamples(data[5:])

        # bed packet should have 4 accel samples per packet (2G)
        samples = [((i / 512.0) * self.accel_g) for i in samples]

        # self._log(" Received 4 accel samples: {}".format(counter-self._lastCounter))
        self._lastCounter = counter
        return counter, [samples[0:3], samples[3:6], samples[6:9], samples[9:]]

    def _parseWristPacket(self, data):
        # same as clip sensor, only 5 packets rolled into one (5 times 20B packets)
        acc, gyr, counter = [], [], -1
        # ignore first received packet since first samples are not correct
        if self._wrist_first_packet_broken_workaround_done < 5:
            self._wrist_first_packet_broken_workaround_done = (
                self._wrist_first_packet_broken_workaround_done + 1
            )
            # print("throw away packet #", self._wrist_first_packet_broken_workaround_done)
            return counter, acc, gyr
        for i in range(0, 5):
            index = i * 20
            if data[index] != 3:
                self._log(
                    "Nonsense packet was received (first byte of value {}) No parsing will happen (length {})".format(
                        data[index], len(data)
                    )
                )
                return
            t_counter, tmp_acc, tmp_gyr = self._parse2Accel2GyroSamples(
                data[index : index + 20]
            )
            if counter < 0:  # return value is first counter
                counter = t_counter
            acc.extend(tmp_acc)
            gyr.extend(tmp_gyr)
        return counter, acc, gyr

    def _parse2Accel2GyroSamples(self, data):
        # clip should have 4G and 2 accel and 2 gyro samples per packet
        # byte 0 is mode ID
        # bytes 1 and 2 are packet counter
        counter = self._decodePacketCounter(data[1:3])
        # byte 3 is battery %
        self._decodeBatteryPercentage(data[3])
        # byte 4 is metadata
        self._decodeMetadata(data[4])
        # till end of packet are 10 bit samples
        samples = self._decode10bitsamples(data[5:])

        # wrist should have 2 accel and 2 gyro samples per packet
        # min/max raw values: accel samples +-512, gyro samples +-500
        acce1xyz = [((i / 512.0) * self.accel_g) for i in samples[0:3]]
        gyro1xyz = [((i / 500.0) * self.gyro_dps) for i in samples[3:6]]

        acce2xyz = [((i / 512.0) * self.accel_g) for i in samples[6:9]]
        gyro2xyz = [((i / 500.0) * self.gyro_dps) for i in samples[9:12]]

        # self._log(" Received 2 accel and 2 gyro samples: {} ".format(counter-self._lastCounter))
        self._lastCounter = counter
        return counter, [acce1xyz, acce2xyz], [gyro1xyz, gyro2xyz]

    def okReceived(self):
        self._log(" ! OK received")
        with self._ok_mutex:
            self._ok_received = True

    def isOkReceived(self):
        with self._ok_mutex:
            prevVal = self._ok_received
            self._ok_received = False
            return prevVal

    def waitForOkReceived(self, timeout=5):
        stopTime = time.time() + timeout
        while time.time() < stopTime:
            if self.isOkReceived():
                return True
            self.peripheral.waitForNotifications(0.1)
        self._log(" ! No OK received!")
        return False

    def handleNotification(self, cHandle, data):

        # workaround for checking "ok" msgs without changing the whole ble interface
        if "ok\\n" in str(data):
            self.okReceived()

        if not self._init_done:
            self._log("Received ble data before init done: {}".format(data))
            return

        timestamp = time.time()
        counter = -1

        # check that read port has been established in init!
        if (
            self._chSerialPortr
            and (cHandle == self._chSerialPortr.valHandle)
            and (len(data) >= 20)
            and (data[0] == 3)
        ):
            samplesAcc, samplesGyr, counter = None, None, -1
            if (self.microHubType == MicroHubType.BED) and (len(data) == 20):
                counter, samplesAcc = self._parse4AccelSamples(list(bytearray(data)))
            elif (self.microHubType == MicroHubType.WRIST) and (len(data) == 100):
                counter, samplesAcc, samplesGyr = self._parseWristPacket(
                    list(bytearray(data))
                )
            elif (self.microHubType == MicroHubType.CLIP) and (len(data) == 20):
                counter, samplesAcc, samplesGyr = self._parse2Accel2GyroSamples(
                    list(bytearray(data))
                )
            else:
                self._log("Issue with parsing data: {}".format(data))
                return
            if counter >= 0:  # there was some substance in received BLE packets
                self._last_M3_packet_received = time.time()
                if not self._time_align:
                    miss_val = [
                        self._MISSING_DATA,
                        self._MISSING_DATA,
                        self._MISSING_DATA,
                    ]
                    if (self.microHubType == MicroHubType.CLIP) or (
                        self.microHubType == MicroHubType.WRIST
                    ):
                        miss_val = [
                            [
                                self._MISSING_DATA,
                                self._MISSING_DATA,
                                self._MISSING_DATA,
                            ],
                            [
                                self._MISSING_DATA,
                                self._MISSING_DATA,
                                self._MISSING_DATA,
                            ],
                        ]
                    self._time_align = TimeAlign(
                        self.sampling_rate,
                        missing_value=miss_val,
                        parent=self,
                        MAX_COUNTER_VALUE=65536,
                    )

                sam_len = len(samplesAcc)
                """timestamp = timestamp - (sam_len*self._time_align._expected_time_between_samples)
                for i in range(sam_len):
                    self._time_align.addSample(timestamp + (i*self._time_align._expected_time_between_samples), counter+i, (samplesAcc[i]) if (self.microHubType == MicroHubType.BED) else [samplesAcc[i], samplesGyr[i]], self)
                """

                # debug print parsed value
                # print(samplesAcc)
                # for s in samplesAcc:
                #    print(s)
                # print("")
                # print(samplesGyr)
                # print(samplesAcc, samplesGyr)

                # NOTE: all samples (from one packet) have the same timestamp since frequency is... variable...
                for i in range(sam_len):
                    self._time_align.addSample(
                        timestamp,
                        counter + i,
                        (samplesAcc[i])
                        if (self.microHubType == MicroHubType.BED)
                        else [samplesAcc[i], samplesGyr[i]],
                        self,
                    )
        else:
            self._log(
                "Received unkown data of length {} on notification:{}".format(
                    len(data), str(data)
                )
            )

    def state_machine_step(self, step_number):
        # init the device
        if step_number == 0:
            self._log(
                "Initalizing uHub {}! MAC: {}".format(
                    self.microHubType.name, self.deviceMac
                )
            )
            # get read and write characteristics for each device type
            def cache_bluepy_ble_peripheral():
                if self._type == MicroHubType.WRIST:
                    self._log("skip caching for wrist")
                    return

                self.peripheral.getServices()
                self.peripheral.getCharacteristics()
                self.peripheral.getDescriptors()
                self._log("BluePy cached services, characteristics and descriptors!")

            try:
                func_timeout(10, cache_bluepy_ble_peripheral)
            except FunctionTimedOut as e:
                self._log(str(e) + "\n Timeout while putting BLE device to cache!")
            except Exception as e:
                self._log(str(e) + "\n Exception while putting BLE device to cache!")

            if self._type == MicroHubType.WRIST:

                def connect():
                    self._log("MTU demand will be sent")
                    # Wrist HW is sending packet of ~100B, just set it to ~larger than that and let it figure it out
                    self.peripheral.setMTU(250)
                    self._log("MTU set")
                    self._chSerialPortw = self._getCharacteristic(
                        constants.bracelet_UUID_data_w, "Data pipe write"
                    )
                    self._log(str(self._chSerialPortw))
                    self._chSerialPortr = self._getCharacteristic(
                        constants.bracelet_UUID_data_r, "Data pipe read"
                    )
                    self._log(str(self._chSerialPortr))
                    # subscribe to the characteristic by writting descriptor
                    self._chSerialPortr.getDescriptors()[0].write(b"\x01\x00", True)

                try:
                    # Timeout the connection attempt to 30 seconds:
                    # when setting MTU and reading characteristics, wristband can be stuck in a BLE state where
                    # connection is established, but bluepy functions never return <- in this way bracelet is crashed, but this thread will continue
                    func_timeout(30, connect)
                except FunctionTimedOut as e:
                    self._log(
                        str(e)
                        + "\n Characteristic write for bracelet was not successful! (bracelet HW BG "
                        "workaround)"
                    )
                    self._chSerialPortr = None
                    self._chSerialPortw = None
                    self._sendToOutputStop(self.deviceMac)
                    self.disconnect_ble()
                    return 0
                for command in constants.WRIST_CONFIG:
                    while self.peripheral.waitForNotifications(0.1):
                        pass
                    if not self._writeAndWait(command, checkOk=True):
                        self.disconnect_ble()
            else:
                self._chSerialPortw = self._getCharacteristic(
                    constants.microhub_UUID_data, "Data RW pipe"
                )
                self._chSerialPortr = self._chSerialPortw
                self._chSerialPortr.getDescriptors()[0].write(b"\x01\x00", True)

                self._writeAndWait(constants.COMM_IDLE, wait=2)
                if MicroHubType.BED == self._type:
                    if not self._writeAndWait(constants.BED_CONFIG, checkOk=True):
                        self.disconnect_ble()
                elif MicroHubType.CLIP == self._type:
                    if not self._writeAndWait(constants.CLIP_CONFIG, checkOk=True):
                        self.disconnect_ble()

            if (not self._chSerialPortr) or (not self._chSerialPortw):
                self._log(
                    "\n\tRead ch is: {}\n\tWrite ch is {}\n therefore we quit!".format(
                        self._chSerialPortr, self._chSerialPortw
                    )
                )
                # send stop signal, since this object is broken (bluepy hangs)
                self._sendToOutputStop(self.deviceMac)
                self.disconnect_ble()
                return 0

            # put to IDLE ("reset" its state)
            self._writeAndWait(constants.COMM_IDLE, wait=2)
            # put device to mode 3
            if not self._writeAndWait(constants.COMM_M3, 0.5, checkOk=True):
                self.disconnect_ble()

            self._lastCounter = -1
            self._init_done = True
            self._last_M3_packet_received = time.time()
            return 1

        self.peripheral.waitForNotifications(1)
        # if no valid samples were received in last 30s issue disconnect (this also handles clip sensor while charging)
        if (time.time() - self._last_M3_packet_received) > 30:
            self._log("No M3 streaming data received in last 30 seconds!")
            self.disconnect_ble()

        return 1


if __name__ == "__main__":
    # test_bed_device="e0:7d:ea:ef:7f:ee"
    # uhub = Microhub(MicroHubType.BED, test_bed_device,None,None,"test_device")

    # test_clip_device="e0:7d:ea:ef:8c:76"
    # test_clip_device = "e0:7d:ea:ef:81:d3"
    # uhub = Microhub(MicroHubType.CLIP, test_clip_device, None, None, "test_device")

    # test_wrist_device="9c:a5:25:12:c6:31"
    # uhub = Microhub(MicroHubType.WRIST,test_wrist_device,None,None,"test_device")

    test_wrist_device = "9c:a5:25:12:c6:31"
    uhub = Microhub(MicroHubType.WRIST, test_wrist_device, None, None, "test_device")

    uhub.startDevice()
    time.sleep(120)
