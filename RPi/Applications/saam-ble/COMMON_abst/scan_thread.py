import time
from datetime import datetime, timezone

from COMMON_abst.device import MsgType
from COMMON_abst.device import DeviceMessage

import threading

from bluepy.btle import Scanner, DefaultDelegate


def minimal_scan(list_names=[], time=10, non_blocking_scan=False):
    retVal = []
    devs = (
        Scanner()
        .withDelegate(DefaultDelegate())
        .scan(timeout=time, passive=non_blocking_scan)
    )
    if len(list_names) == 0:
        retVal = devs
    else:
        for name in list_names:
            retVal.extend(
                list(
                    filter(
                        lambda d: str(d.getValue(9)).lower().startswith(name.lower()),
                        devs,
                    )
                )
            )

    return list(
        map(
            lambda d: {
                "name": str(d.getValue(9)),
                "mac": str(d.addr),
                "addrType": d.addrType,
                "rssi": d.rssi,
            },
            retVal,
        )
    )


class ScannerWorker(DefaultDelegate, threading.Thread):
    """
    This is a standalone scanner that returnes scanned items via DeviceMessage via DeviceQUeue
    """

    def __init__(self, outQueue, log=None, iface=None):
        DefaultDelegate.__init__(self)
        threading.Thread.__init__(self, name="BleScanThread")
        self._threadLock = threading.Lock()
        self._parentComm = outQueue
        self._devices = set()
        self._timeBetweenScans = 10
        self._timeToScan = 2
        self._running = True
        self._log = log
        self.interface = iface

    def addMacToScan(self, mac):
        # self._logger("Adding {} to scan list.".format(mac))
        with self._threadLock:
            self._devices.add(mac)
            self._logger("Now scanning for: {}".format(str(self._devices)))

    def stopThread(self):
        self._running = False

    def _logger(self, text):
        if self._log:
            self._log.debug(
                "%s SCANNER: %s",
                datetime.now(timezone.utc).strftime("%Y_%m_%d %T"),
                text,
            )

    def run(self):
        self._running = True
        self._logger("Starting scan thread")
        time.sleep(1)  # wait for main thread to parse config and prepare devices
        while self._running:
            # only scan if any devices need to be searched for
            num_elems = 0
            devs = []
            with self._threadLock:
                num_elems = len(self._devices)
            if num_elems > 0:
                try:
                    scanner = Scanner(iface=self.interface).withDelegate(self)
                    devs = scanner.scan(self._timeToScan, True)
                except Exception as exc:
                    self._logger("Scanning exception:" + str(exc))

            for dev in devs:
                # print("\t", dev.addr.upper(), (dev.addr.upper() in (name.upper() for name in self._devices)))
                if (
                    dev.addr.upper() in (name.upper() for name in self._devices)
                ) and self._parentComm:
                    self._logger(
                        "Notifying parent thread that device {} is found".format(
                            dev.addr
                        )
                    )
                    self._parentComm.push(
                        DeviceMessage(None, MsgType.ScanResult, data=dev.addr)
                    )
                    #  Scanning really doesn't hurt us -> constantly scan for all devices...
                    # with self._threadLock:
                    # self._devices.discard(dev.addr)
            # sleep this thread for predetermined time
            time.sleep(self._timeBetweenScans)
        # somebody called stopThread
        self._logger("Stopping scan thread")
