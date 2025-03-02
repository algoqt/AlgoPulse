from gm.api import *
import logging
import os
import datetime
from contextlib import contextmanager
import time

# 设置API密钥
token_id = 'your_token_id'

# 设置策略Id
strategy_id = 'your_strategy_id'

DATA_PATH = "D:/gmData"


TRADE_CALENDAR_FILE="{DATA_PATH}/TradeCalendar.h5".format(DATA_PATH=DATA_PATH)

STATICINFO_FILE="{DATA_PATH}/StaticInfo.h5".format(DATA_PATH=DATA_PATH)

TICK_FILE_PATH_PATTERN=DATA_PATH + "/Tick{dtstr}.h5"

MINUTEBAR_FILE="{DATA_PATH}/MinuteBar.h5".format(DATA_PATH=DATA_PATH)

DAILYBAR_FILE="{DATA_PATH}/DailyBar.h5".format(DATA_PATH=DATA_PATH)

def get_Logger(logger_name,loggingLevel=logging.INFO,logFile=None):

    #logging.setLogRecordFactory(custom_log_record_factory)
    logger = logging.getLogger(logger_name)
    logger.setLevel(loggingLevel)

    if not logFile:
        today = datetime.datetime.now().strftime("%Y%m%d")
        logDir = "./logs"
        if not os.path.exists(logDir):
            os.makedirs(logDir)
        logFile = os.path.join(logDir,"%s_%s.log" % (logger_name,today))

    fh = logging.FileHandler(logFile,encoding='utf-8')
    fh.setLevel(loggingLevel)
    fmt = "[%(asctime)s.%(msecs)03d][%(process)d][%(thread)d][%(levelname)s][%(filename)s:%(lineno)d]:%(message)s"  # %(filename)s[%(lineno)d]  %(filename)s
    datefmt = "%Y%m%d %H:%M:%S"
    formatter = logging.Formatter(fmt, datefmt)
    fh.setFormatter(formatter)
    logger.addHandler(fh)

    sh = logging.StreamHandler()
    sh.setLevel(loggingLevel)
    sh.setFormatter(formatter)
    logger.addHandler(sh)

    return logger


mainLogger = get_Logger("main")

@contextmanager
def timeCount(name: str):
    s = time.time()
    yield
    elapsed = time.time() - s
    mainLogger.info(f'[{name}] {elapsed: .3f} sec')