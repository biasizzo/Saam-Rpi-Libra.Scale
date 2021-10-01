from enum import IntEnum
from bluepy.btle import UUID


class MicroHubType(IntEnum):
    BED = 1
    CLIP = 2
    WRIST = 3


"""characteristic UUIDs"""
microhub_UUID_name = 0x2A00

# v1 (prototype)
microhub_UUID_data = 0xFFE1

# v2 (release)
bracelet_UUID_data_r = UUID("0003cdd1-0000-1000-8000-00805f9b0131")
bracelet_UUID_data_w = UUID("0003cdd2-0000-1000-8000-00805f9b0131")

# commands
COMM_IDLE = b"IDLE\n"
COMM_M3 = b"M3\n"

# BED
BED_CONFIG = b"SET200\n"

# CLIP
CLIP_CONFIG = b"SET213\n"

# WRISTBAND
WRIST_CONFIG = [COMM_IDLE, b"DMP0\n", b"PERIOD020\n", b"DPS3\n", b"FSR1\n"]


def checkConfig(microHubType: MicroHubType, sampling_rate, accel_g, gyro_dps):
    # return true if metadata is as expected per device
    if microHubType == MicroHubType.BED:
        if (sampling_rate == 56.25) and (accel_g == 2) and (gyro_dps == 250):
            return True
    if microHubType == MicroHubType.CLIP:
        if (sampling_rate == 56.25) and (accel_g == 4) and (gyro_dps == 2000):
            return True
    if microHubType == MicroHubType.WRIST:
        if (sampling_rate == 56.25) and (accel_g == 4) and (gyro_dps == 2000):
            return True
    return False
