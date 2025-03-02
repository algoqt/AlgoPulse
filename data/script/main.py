# coding=utf-8
from __future__ import print_function, absolute_import
from functools import partial
from multiprocessing import Pool, cpu_count
from typing import Dict, List, NoReturn, Text, Tuple
from gm.api import *
from gm.csdk.c_sdk import BarLikeDict2, TickLikeDict2
from gm.model import DictLikeAccountStatus, DictLikeExecRpt, DictLikeIndicator, DictLikeOrder, DictLikeParameter
from gm.pb.account_pb2 import AccountStatus, ExecRpt, Order
from gm.pb.performance_pb2 import Indicator
from gm.pb.rtconf_pb2 import Parameter
from gm.utils import gmsdklogger
import numpy as np
import pandas as pd
from tableDefs import *
from staticInfoJob import downloadStaticInfo
from dailyBarJob   import downloadDailyBar
from minuteBarJob  import downloadMinuteBar
from tickJob import downloadTicks
from config import *
from config import mainLogger

"""
本示例用于说明python sdk 当前支持的回调方法示例. 
不具有业务含义, 只用于策略编写参考
注：
建议使用python3.6.5以上的版本, gmsdk 支持Python3.6.x, python3.7.x, python3.8.x, python3.9.x 
"""

def task_downloadStaticInfo(context,overwrite=False):
    mainLogger.info(f"begin task_downloadStaticInfo,{context.now}")
    datestr = context.now.strftime("%Y-%m-%d")
    context.dts = get_trading_dates(exchange="SZSE",start_date=datestr,end_date=datestr)
    for dt in context.dts:
        symbolInfos = get_instruments(exchanges=["SZSE","SHSE"],sec_types=[SEC_TYPE_BOND_CONVERTIBLE
                                                                           ,SEC_TYPE_STOCK,SEC_TYPE_OPTION
                                                                           ,SEC_TYPE_FUND,SEC_TYPE_INDEX],skip_st=False,skip_suspended=False, df=True)
        mainLogger.info(f"{dt} query symbol size :{len(symbolInfos)}")
        downloadStaticInfo(symbolInfos["symbol"].to_list(),dt,dt,True)


def task_downloadDailyBar(context):
    mainLogger.info(f"begin task_downloadDailyBar,{context.now}")
    datestr = context.now.strftime("%Y-%m-%d")
    context.dts = get_trading_dates(exchange="SZSE",start_date=datestr,end_date=datestr)
    for dt in context.dts:
        downloadDailyBar([],dt,dt)

def task_downloadTicks(context):
    
    mainLogger.info(f"begin task_downloadTicks,{context.now}")
    datestr    = context.now.strftime("%Y-%m-%d")
    start_time = datestr + " 09:15:00"
    end_time   = datestr + " 15:00:10"
    mainLogger.info(f"date:{datestr},start_time:{start_time} end_time:{end_time}")
    context.dts = get_trading_dates(exchange="SZSE",start_date=datestr,end_date=datestr)

    context.h5files  = {}  # Dict[str,tb.File] = {}
    for dt in context.dts:
        dtstr = dt.replace("-","")
        with tb.open_file(TICK_FILE_PATH_PATTERN.format(dtstr=dtstr), mode="a"):
            pass
        
        with tb.open_file(STATICINFO_FILE, mode='r') as store:
            node_key = "/I{}".format(dtstr)
            ssinfos = store.get_node(node_key).read()
            data    = pd.DataFrame(ssinfos)
            data["symbol"]=data["symbol"].apply(lambda x:x.decode())
            symbolInfos= data[(data.is_suspended==0)]
        symbols = []
        with tb.open_file(TICK_FILE_PATH_PATTERN.format(dtstr=dtstr), mode='r') as store:
            for symbol in symbolInfos["symbol"].to_list():
                _tick_key = "/" + symbol.replace(".","")
                if not store.__contains__(_tick_key):
                    symbols.append(symbol)

        mainLogger.info(f"{datestr} will query symbol counts :{len(symbols)}")
        start_time = dt + " 09:15:00"
        end_time   = dt + " 15:00:10"
        downloadTicks(context,symbols,start_time,end_time)
        mainLogger.info(f"{datestr} query symbol ticks done :{len(symbols)}")

    task_downloadDailyBar(context)

def task_downloadMinuteBar(context):
    datestr    = context.now.strftime("%Y-%m-%d")
    mainLogger.info(f"task_downloadMinuteBar:{datestr}")
    context.dts = get_trading_dates(exchange="SZSE",start_date=datestr,end_date=datestr)
    for dt in context.dts:
        downloadMinuteBar(dt)

def init(context):
    # type: (Context) -> NoReturn
    """
    策略中必须有init方法,且策略会首先运行init定义的内容，可用于
    * 获取低频数据(get_fundamentals, get_fundamentals_n, get_instruments, get_history_instruments, get_instrumentinfos,
    get_constituents, get_history_constituents, get_sector, get_industry, get_trading_dates, get_previous_trading_date,
    get_next_trading_date, get_dividend, get_continuous_contracts, history, history_n, )
    * 申明订阅的数据参数和格式(subscribe)，并附带数据事件驱动功能
    * 申明定时任务(schedule)，附带本地时间事件驱动功能
    * 读取静态的本地数据或第三方数据
    * 定义全局常量,如 context.user_data = 'balabala'
    * 最好不要在init中下单(order_volume, order_value, order_percent, order_target_volume, order_target_value, order_target_percent)
    """

    # 示例定时任务: 每天 14:50:00 调用名字为 my_schedule_task 函数

    task_downloadStaticInfo(context)

    schedule(schedule_func=task_downloadStaticInfo, date_rule='1d', time_rule='09:10:00')

    schedule(schedule_func=task_downloadMinuteBar, date_rule='1d', time_rule='17:10:00')

    schedule(schedule_func=task_downloadTicks, date_rule='1d', time_rule='17:20:00')

    schedule(schedule_func=task_downloadDailyBar, date_rule='1d', time_rule='19:55:00')


def on_tick(context, tick):
    # type: (Context, TickLikeDict2) -> NoReturn
    """
    tick数据推送事件
    参数 tick 为当前被推送的tick.
    tick包含的key值有下列值.
    symbol              str                   标的代码
    open                float                 日线开盘价
    high                float                 日线最高价
    low                 float                 日线最低价
    price               float	              最新价
    cum_volume          long                  成交总量/最新成交量,累计值
    cum_amount          float                 成交总金额/最新成交额,累计值
    trade_type          int                   交易类型 1: ‘双开’, 2: ‘双平’, 3: ‘多开’, 4: ‘空开’, 5: ‘空平’, 6: ‘多平’, 7: ‘多换’, 8: ‘空换’
    last_volume         int                   瞬时成交量
    cum_position        int                   合约持仓量(期),累计值（股票此值为0）
    last_amount         float                 瞬时成交额
    created_at          datetime.datetime     创建时间
    quotes              list[Dict]            股票提供买卖5档数据, list[0]~list[4]分别对应买卖一档到五档, 期货提供买卖1档数据, list[0]表示买卖一档. 目前期货的 list[1] ~ list[4] 值是没有意义的
        quotes 里每项包含的key值有:
          bid_p:  float   买价
          bid_v:  int     买量
          ask_p   float   卖价
          ask_v   int     卖量

    注: 可以使用属性访问的方式得到相应的key的值. 如要访问: symbol. 则可以使用 tick.symbol 或 tick['symbol']
    访问quote里的bid_p, 则可以使用 tick.quotes[0].bid_p  或 tick['quotes'][0]['bid_p']
    """
    pass


def on_bar(context, bars):
    # type: (Context, List[BarLikeDict2]) -> NoReturn
    """
    bar数据推送事件
    参数 bars 为当前被推送的bar列表. 在调用subscribe时指定 wait_group=True, 则返回的是到当前已准备好的bar列表; 若 wait_group=False, 则返回的是当前推送的 bar 一个对象, 放在在 list 里
    bar 对象包含的key值有下列值.
    symbol         str      标的代码
    frequency      str      频率, 支持多种频率. 要把时间转换为相应的秒数. 如 30s, 60s, 300s, 900s
    open           float    开盘价
    close          float    收盘价
    high           float    最高价
    low            float    最低价
    amount         float    成交额
    volume         long     成交量
    position       long     持仓量（仅期货）
    bob            datetime.datetime    bar开始时间
    eob            datetime.datetime    bar结束时间

    注: 可以使用属性访问的方式得到相应的key的值. 如要访问: symbol. 则可以使用 bar.symbol 或 bar['symbol']
    """
    pass


def on_backtest_finished_v2(context, indicator):
    # type: (Context, Indicator) -> NoReturn
    """
    回测结束事件. 参数 indicator 为此次回测的绩效指标
    3.0.113 后增加
    已 on_backtest_finished 具有同等意义, 在二者都被定义时(当前函数返回类型为类，速度更快，推介使用), 只会调用 on_backtest_finished_v2
    """
    pass


def on_error(context, code, info):
    # type: (Context, int, Text) -> NoReturn
    """
    底层sdk出错时的回调函数
    :param context:
    :param code: 错误码.  参考: https://www.myquant.cn/docs/python/python_err_code
    :param info: 错误信息描述
    """
    # tdate = context.now.strftime("%Y-%m-%d %H:%M:%S")
    # mainLogger.error(f"{tdate} error:{code},info:{info}")
    pass


def on_shutdown(context):
    # type: (Context) -> NoReturn
    """
    策略退出前回调
    注：只有在终端点击策略·断开·按钮才会触发，直接关闭策略控制台不会被调用
    """
    pass


if __name__ == '__main__':
    '''
        strategy_id策略ID, 由系统生成
        filename文件名, 请与本文件名保持一致
        mode运行模式, 实时模式:MODE_LIVE回测模式:MODE_BACKTEST
        token绑定计算机的ID, 可在系统设置-密钥管理中生成
        backtest_start_time回测开始时间
        backtest_end_time回测结束时间
        backtest_adjust股票复权方式, 不复权:ADJUST_NONE前复权:ADJUST_PREV后复权:ADJUST_POST
        backtest_initial_cash回测初始资金
        backtest_commission_ratio回测佣金比例
        backtest_slippage_ratio回测滑点比例
    '''

    run(strategy_id=strategy_id,
        filename='main.py',
        mode=MODE_BACKTEST,
        token= token_id,
        backtest_start_time='2025-02-26 08:00:00',
        backtest_end_time='2025-02-26 21:00:00',
        backtest_adjust=ADJUST_PREV,
        backtest_initial_cash=10000000,
        backtest_commission_ratio=0.0001,
        backtest_slippage_ratio=0.0001)



