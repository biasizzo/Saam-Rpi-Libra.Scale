import os

import logging
import logging.handlers


def get_logger(location_id, logSize = 1, backlog_count=1):
    # directory where logs will be saved
    log_directory = "/var/log/SAAM/"
    try:
        if not os.path.exists(log_directory):
            os.makedirs(log_directory)
            print("Creating directory:" + log_directory)
    except Exception as e:
        print(e)

    absolute_log_path = log_directory + location_id + "_log.txt"
    print("Opening log:", absolute_log_path)

    logger = logging.getLogger(location_id + "log")
    if len(logger.handlers) == 0:
        logger.setLevel(logging.DEBUG)
        # rotating (logSize*1M) of log, with 2 backups
        logger.addHandler(logging.handlers.RotatingFileHandler(absolute_log_path, maxBytes= (logSize * 1000000), backupCount=backlog_count))

    return logger
