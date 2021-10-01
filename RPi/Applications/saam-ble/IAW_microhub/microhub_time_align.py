import time

_DEBUG_TIME_ALIGN = False
_MINIMAL_ALLOWED_FREQ = 40  # below 40Hz signal is useless for movement detection
_MAXIMUM_ALLOWED_FREQ = 150  # well, that's just plain wrong...What we demand from the device, and what is sensible for BLE throughput


class TimeAlign:
    def __init__(
        self, sampling_frequency, parent, missing_value=None, MAX_COUNTER_VALUE=65536
    ):
        self._missing_value = missing_value
        self._fs = sampling_frequency
        self._samples = []
        self._parent = parent  # if this is none, it will crash
        self.MAX_COUNTER_VALUE = MAX_COUNTER_VALUE

    @property
    def _expected_time_between_samples(self):
        if self._fs <= 0:
            return float("nan")
        return 1.0 / self._fs

    def _last_sample_info(self):
        if len(self._samples) > 0:
            return self._samples[-1]["t"], self._samples[-1]["c"]
        return 0, -1

    def addSample(self, timestamp, counter, data, parent):

        counter = counter % self.MAX_COUNTER_VALUE
        last_t, last_c = self._last_sample_info()

        if _DEBUG_TIME_ALIGN:
            self._parent._log(
                "Received data with timestamp: %s\tcounter: %s\tdiff: %s",
                "{0:.2f}".format(timestamp),
                counter,
                counter - last_c,
            )

        # at least one sample is present in queue
        if last_c >= 0:
            diff = counter - last_c
            # if diff is < 0 -> counter overflow occured, fix this
            if diff < 0:
                diff = counter - (last_c - self.MAX_COUNTER_VALUE)
            # use counters to fill in missing samples (lost ble packets)
            if diff > 1:
                for i in range(diff - 1):
                    c_curr = (last_c + 1 + i) % self.MAX_COUNTER_VALUE
                    self._samples.append(
                        {"t": last_t, "c": c_curr, "s": self._missing_value}
                    )
        # append current sample
        self._samples.append({"t": timestamp, "c": counter, "s": data})

        if len(self._samples) > 0:
            curr_time = (
                time.time() - 2
            )  # 2 second delay when sending data (tiny buffer for BLE delays)
            first_sample_time = self._samples[0]["t"]
            if (
                curr_time - first_sample_time
            ) >= 10.0:  # at least 10 (+2) seconds has passed from first sample in the list
                # iteratively get num samples to send (back to front)
                num_samples_to_send = 0
                for i in range(len(self._samples) - 1, -1, -1):
                    # TODO awaiting confirmation from E8 if this needs to be changed (and in what way)
                    if self._samples[i]["t"] < curr_time:
                        num_samples_to_send = i
                        break

                timestep = (
                    self._samples[num_samples_to_send]["t"] - self._samples[0]["t"]
                )  # calc timestep (only used for logging)
                if timestep > 0:
                    curr_freq = round(num_samples_to_send / timestep, 3)
                    if (curr_freq < _MINIMAL_ALLOWED_FREQ) or (
                        curr_freq > _MAXIMUM_ALLOWED_FREQ
                    ):
                        del self._samples[:num_samples_to_send]  # remove samples
                        self._parent._log(
                            "  [TA] frequency was {} ! Removed {} samples!".format(
                                timestep, num_samples_to_send
                            )
                        )
                        # we're disconnecting immediatly one is out of bounds
                        self._parent.disconnect_ble()
                        return
                    # send relevant samples
                    parent._sendToOutputDev(
                        {"data": self._samples[0:num_samples_to_send]}
                    )
                    # verbose log what was sent
                    self._parent._log(
                        "  [TA] {} in {} [{}Hz]\t{}".format(
                            num_samples_to_send,
                            round(timestep, 3),
                            curr_freq,
                            counter,
                        )
                    )
                else:
                    self._parent._log("  [TA] timestep == {}  !!!!!".format(timestep))

                del self._samples[:num_samples_to_send]  # remove sent samples
