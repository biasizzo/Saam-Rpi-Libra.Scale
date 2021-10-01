from enum import IntEnum
from json import JSONEncoder
import time

from threading import Thread as ProcessImplementation
from queue import Queue as QueueImplementation

class MsgType(IntEnum):
    """Msg type enumerator.

    [data contains string or None]
    EmptyMessage - this message can safely be discarded

    LogInfo - data contains log information
    LogDebug - data contains debug information
    LogError - data contains error information

    [data contains device specific payload]
    Start - General command to Device to connect and start sampling
    Stop - General command to Device to stop sampling
    Restart - General command to Device to restart itself and sampling process

    DeviceStatus - contains general status of device
    DeviceError - contains error from lower layers connecting to device
    DeviceSpecific - contains any message from device
    """

    EmptyMessage = 0    # used for error reporting or weird cases where msg is expected
    LogInfo = 1
    LogDebug = 2
    LogError = 3
    Start = 4
    Stop = 5
    Restart = 6
    ScanResult = 100
    DeviceStatus = 253
    DeviceError = 254
    DeviceSpecific = 255

class DeviceMessage():
    """Simple device message class, used by DeviceQueue."""

    def __init__(self, sender, msgType, timestamp=time.time(), data=None):
        """Initialize Device to default values."""
        if not isinstance(msgType, MsgType):
            raise Exception("MsgType: type of message is not correct!")
        if sender and not (hasattr(sender, "deviceMac") and hasattr(sender, "deviceType")):
            raise Exception("Sender must have attributes deviceMac and deviceType")
        self._msgType = msgType
        self._sender = sender
        self._timestamp = timestamp
        # if data is present it should be in a json-like format, or string if logging
        self._data = data

    @property
    def sender(self) -> 'Device':
        return self._sender

    @property
    def msgType(self) -> MsgType:
        return self._msgType

    @property
    def timestamp(self) -> float:
        return self._timestamp

    @property
    def data(self):
        return self._data

    def dataToJson(self) -> str:
        if not self.data:
            return ""
        return JSONEncoder().encode(self.data)


class DeviceQueue():
    """SAAM implementation of queue.

    It uses whatever underlying queue we wish, and simplifies queue interface to three functions (push, pop, pop_blocking) that do not throw Exceptions (if correct parameters are given)

    push - add DeviceMessage to queue
    pop - get DeviceMessage from queue (returns DeviceMessage of type EmptyMessage if queue is empty)
    pop_blocking - get DeviceMessage from queue, and block for 'timeout' seconds if queue is currently empty (returns DeviceMessage of type EmptyMessage if queue is empty after timeout)
    empty - return True if queue is empty, if 'timeout' is given, function will block for 'timeout' seconds and then check
    size - return number of DeviceMessages in, if 'timeout' is given, function will block for 'timeout' seconds and then check
    """

    def __init__(self, parent=None):
        """Initialize queue with default paramters."""
        self.__parent = parent
        self.__queue = QueueImplementation()

    @property
    def parent(self) -> 'Device':
        """Parent (owner) of the queue. Could be set to None"""
        return self.__parent

    def pop(self) -> DeviceMessage:
        """Non-blocking pop from queue.

        If queue is empty it will return DeviceMessage of type EmptyMessage"""
        try:
            return self.__queue.get(False)
        except Exception:
            return DeviceMessage(self.parent, MsgType.EmptyMessage, data="No message in queue")

    def pop_blocking(self, timeout=None) -> DeviceMessage:
        """Blocking pop from queue.

        If no message is queued before calling or during timeout period, it will return DeviceMessage of type EmptyMessage.
        If message is queued during timeout period, this function returns before timeout period is finished"""
        try:
            return self.__queue.get(True, timeout)
        except Exception:
            return DeviceMessage(self.parent, MsgType.EmptyMessage, data="No message in queue")

    def push(self, msg: DeviceMessage):
        """Non-blocking push to queue."""
        if not isinstance(msg, DeviceMessage):
            raise Exception("Queue: Msg is not of correct type")
        self.__queue.put_nowait(msg)

    def empty(self, timeout=None):
        """Return True if queue is empty.

        If timeout > 0 this wil block thread for timeout seconds, before returning value
        """
        if timeout:
            time.sleep(timeout)
        return self.__queue.empty()

    def size(self, timeout=None) -> int:
        """Return number of messages in queue.

        If timeout > 0 this wil block thread for timeout seconds, before returning value
        """
        if timeout:
            time.sleep(timeout)
        return self.__queue.qsize()
