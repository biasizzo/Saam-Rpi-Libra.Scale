import re  # regex

from datetime import datetime, timezone

from COMMON_abst.messaging import (
    ProcessImplementation,
    DeviceQueue,
    DeviceMessage,
    MsgType,
)

from bluepy.btle import (
    Peripheral,
    DefaultDelegate,
    Characteristic,
    ADDR_TYPE_PUBLIC,
    ADDR_TYPE_RANDOM,
    UUID,
)

from time import sleep as thread_sleep

from func_timeout import func_timeout, FunctionTimedOut


class _Dev_callback_Impl(DefaultDelegate):
    @staticmethod
    def has_method(o, name):
        return callable(getattr(o, name, None))

    def __init__(self, receiver: "Device" = None):
        DefaultDelegate.__init__(self)
        if receiver and _Dev_callback_Impl.has_method(receiver, "handleNotification"):
            self._receiver = receiver
        else:
            raise (Exception("This class has no handleNotification function!"))

    def handleNotification(self, cHandle, data):
        self._receiver.handleNotification(cHandle, data)


"""Extend peripheral to include workaround when bluepy helper is stuck """


class SafePeripheral(Peripheral):
    def __init__(self):
        super().__init__()

    def disconnect(self):
        try:
            func_timeout(5, super().disconnect)
        except BrokenPipeError:
            #print("Broken pipe")
            if self._helper:
                #print("SafePer terminating helper")
                self._helper.terminate()  # forcefully close helper
                self._helper.wait(0.001)
            self._helper = None
        except FunctionTimedOut:
            #print("Timed out")
            if self._helper:
                #print("SafePer terminating helper")
                self._helper.terminate()  # forcefully close helper
                self._helper.wait(0.001)
            self._helper = None

        except Exception as e:
            #print("Exception")
            print(str(e))


"""SAAM device interface module."""


class Device(ProcessImplementation):
    def __init__(
        self,
        name,
        deviceType,
        deviceMac,
        outputQueue: DeviceQueue = None,
        args=(),
        iface=None,
        device_addr_public: bool = True,
        logger=None,
    ):
        """Initialize device

        Every device extends this class, and provides it's run function

        private variables accessed via property decorator, so they are not changed after creation
        """
        ProcessImplementation.__init__(self, name=name, args=args)

        # set up regex that only keeps hex characters
        self._regex = re.compile("[^0123456789abcdef]")

        # if no mac address is given, give up
        if not deviceMac:
            raise Exception("DEV: mac address is not known!")

        self.__deviceMac = deviceMac.lower()
        self.__deviceType = deviceType
        self.__deviceIface = iface
        self.__device_addr_type = (
            ADDR_TYPE_PUBLIC if device_addr_public else ADDR_TYPE_RANDOM
        )
        self._logger = logger

        # set up outbound comm channel
        self.__output = outputQueue

        # set up ble device
        self._peripheral = SafePeripheral().withDelegate(_Dev_callback_Impl(self))

        # set up receiving queue
        self.__input = DeviceQueue()

    @property
    def iface(self):
        return self.__deviceIface

    @property
    def deviceMac(self):
        return self.__deviceMac

    @property
    def deviceMacMinimal(self):
        return self._regex.sub("", self.__deviceMac)

    @property
    def deviceType(self):
        return self.__deviceType

    @property
    def input(self) -> DeviceQueue:
        return self.__input

    @property
    def output(self) -> DeviceQueue:
        return self.__output

    @staticmethod
    def has_method(o, name):
        return callable(getattr(o, name, None))

    def _sendToOutput(self, msg: DeviceMessage):
        if not self.output:
            self._log("DEV: Output queue is not instanced")
            return
        if not msg.sender:
            msg._sender = self
        self.output.push(msg)

    def _log(self, msg):
        if False:
            print(datetime.now(timezone.utc).strftime("%Y_%m_%d %T %f"), msg)
        if self._logger:
            self._logger.debug(
                "%s %s", datetime.now(timezone.utc).strftime("%Y_%m_%d %T %f"), msg
            )

    # helpers for logging (shorhand for log string to output queue)
    def _sendToOutputInfo(self, msg):
        self._sendToOutput(DeviceMessage(self, MsgType.LogInfo, data=msg))

    def _sendToOutputDebug(self, msg):
        self._sendToOutput(DeviceMessage(self, MsgType.LogDebug, data=msg))

    def _sendToOutputError(self, msg):
        self._sendToOutput(DeviceMessage(self, MsgType.LogError, data=msg))

    def _sendToOutputDev(self, msg):
        self._sendToOutput(DeviceMessage(self, MsgType.DeviceSpecific, data=msg))

    def _sendToOutputStop(self, msg):
        self._sendToOutput(DeviceMessage(self, MsgType.Stop, data=msg))

    def startDevice(self):
        """Create process if not existing, and send START signal via queue"""
        if not self.is_alive():
            self.start()
            self._log("DEV: Starting device thread {}".format(self.name))

    def stopDevice(self, blocking=False):
        """Send stop signal if running, and wait for join (if blocking stop is demanded)"""
        self._log("DEV: Stop msg to {}".format(self.name))
        if self.is_alive():
            self.__input.push(DeviceMessage(self, MsgType.Stop))
        if blocking:
            self._log("DEV: Waiting for thread {}".format(self.name))
            self.join()  # wait for thread to stop
        self._log("DEV: {} considered stopped".format(self.name))

    def msgToDevice(self, msg: DeviceMessage):
        self.__input.push(msg)

    @property
    def peripheral(self) -> SafePeripheral:
        return self._peripheral

    @property
    def is_connected(self):
        return self._is_connected()

    def _is_connected(self, _recursive_depth=10):
        try:
            if self._peripheral.getState() == "conn":
                return True
        except Exception as e:
            # this is a workaround for when dbus is very very busy, and getState is blocked by waiting messages on the bus
            if (
                (len(e.args) >= 2)
                and ("rsp" in e.args[1])
                and (e.args[1]["rsp"][0] == "ntfy")
            ):
                while self._peripheral.waitForNotifications(0.001):
                    pass
                return self._is_connected(_recursive_depth - 1)
            if _recursive_depth == 0:
                self._log("Exception while checking connection state!")
        return False

    def disconnect_ble(self):
        """ call is_connected to clear dbus
        if congested bunch of messages might be missed or disc command 
        will never be fired"""
        self._log("Disconnect called on the device!")
        self._peripheral.disconnect()

        """additional check/workaund for bluepy hanging, where we definitely kill the connection"""
        thread_sleep(0.5)  # wait for disconnect to do its thing
        helper = self.peripheral._helper
        if helper:
            self._log("Terminate helper!")
            helper.terminate()
            helper.wait(0.01)

    def run(self):
        # run indefinetly until device is connected
        if not Device.has_method(self, "state_machine_step"):
            self._log('Device Implementation has no "state_machine_step" function!')
            return

        # connect ble peripheral
        try:
            self.peripheral.connect(
                self.__deviceMac, self.__device_addr_type, self.__deviceIface
            )
        except:
            pass

        state_machine = 0
        sent_stop = False
        while self.is_connected:
            try:
                state_machine = func_timeout(
                    120, self.state_machine_step, args=[state_machine]
                )
            except FunctionTimedOut as e:
                self._log(
                    "Exiting device thread! State machine took too long! \n{}".format(
                        str(e)
                    )
                )
                self._sendToOutputStop(self.deviceMac)
                sent_stop = True
                break
            except Exception as e:
                self._log("Exception occured while connected\n{}".format(str(e)))
                self.disconnect_ble()
                break
        else:
            if state_machine == 0:
                self._log("Unsuccessful connection attempt! (state_machine == 0)")
            else:
                self._log("is_connected() func returned False (or error happened)!")

        try:
            func_timeout(
                30, self.disconnect_ble
            )  # if there was an error, wrap this in timeout just in case
        except FunctionTimedOut:
            pass  # there is nothing we can do at this point...
        self._log("Exiting device thread!")
        if not sent_stop:
            self._sendToOutputStop(self.deviceMac)

    @property
    def services(self):
        return self._peripheral.services

    def handleNotification(self, cHandle, data):
        self._log(
            "Received notification from handle [ REIMPLEMENT THIS! ] {}".format(cHandle)
        )
        pass

    def state_machine_step(self, step_number):
        self._log(
            "uHub state machine running [ REIMPLEMENT THIS! ] {}".format(step_number)
        )
        thread_sleep(1)
        return 0

    def _getCharacteristic(self, uuid, description=None) -> Characteristic:
        uuid = UUID(uuid)
        chs = self.peripheral.getCharacteristics(uuid=uuid)
        if len(chs) > 0:
            return chs[0]
        else:
            return None
