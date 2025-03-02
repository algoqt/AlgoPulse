
import warnings
import tables as tb
warnings.filterwarnings('ignore', category=tb.NaturalNameWarning)
from os.path import expanduser
from config import *


class TradeCalendar(tb.IsDescription):
    date = tb.Int32Col(pos=0)
    trade_date = tb.Int32Col(pos=1)
    next_trade_date = tb.Int32Col(pos=2)
    pre_trade_date  = tb.Int32Col(pos=3)

class Tick(tb.IsDescription):
    symbol  = tb.UInt32Col(pos=0)
    exchange= tb.UInt8Col(pos=1)
    open    = tb.Float32Col(pos=2)
    high    = tb.Float32Col(pos=3)
    low     = tb.Float32Col(pos=4)
    price         = tb.Float32Col(pos=5)
    bid_price1    = tb.Float32Col(pos=6)
    bid_volume1   = tb.UInt64Col(pos=7)
    bid_price2    = tb.Float32Col(pos=8)
    bid_volume2   = tb.UInt64Col(pos=9)
    bid_price3    = tb.Float32Col(pos=10)
    bid_volume3   = tb.UInt64Col(pos=11)
    bid_price4    = tb.Float32Col(pos=12)
    bid_volume4   = tb.UInt64Col(pos=13)
    bid_price5    = tb.Float32Col(pos=14)
    bid_volume5   = tb.UInt64Col(pos=15)
    ask_price1    = tb.Float32Col(pos=16)
    ask_volume1   = tb.UInt64Col(pos=17)
    ask_price2    = tb.Float32Col(pos=18)
    ask_volume2   = tb.UInt64Col(pos=19)
    ask_price3    = tb.Float32Col(pos=20)
    ask_volume3   = tb.UInt64Col(pos=21)
    ask_price4    = tb.Float32Col(pos=22)
    ask_volume4   = tb.UInt64Col(pos=23)
    ask_price5    = tb.Float32Col(pos=24)
    ask_volume5   = tb.UInt64Col(pos=25)
    cum_volume    = tb.UInt64Col(pos=26)
    cum_amount    = tb.Float64Col(pos=27)
    last_amount   = tb.Float64Col(pos=28)
    last_volume   = tb.UInt64Col(pos=29)
    created_at    = tb.UInt32Col(pos=30)


class StockInfo(tb.IsDescription):
    trade_date = tb.Int32Col(pos=0)  # 交易日期
    symbol = tb.StringCol(11, pos=1)  # 股票代码
    sec_level = tb.Int32Col(pos=2)  # 股票级别
    sec_type = tb.Int32Col(pos=3)  # 证券类型
    exchange = tb.StringCol(4, pos=4)  # 交易所
    sec_id = tb.StringCol(6, pos=5)  # 交易所证券代码
    sec_name = tb.StringCol(64, pos=6)  # 证券名称
    upper_limit = tb.Float32Col(pos=6)  # 涨停价
    lower_limit = tb.Float32Col(pos=7)  # 跌停价
    pre_close = tb.Float32Col(pos=8)  # 昨收盘价
    adj_factor = tb.Float32Col(pos=9)  # 复权因子
    is_suspended = tb.Int8Col(pos=10)  # 是否停牌
    price_tick      = tb.Float32Col(pos=11)  # 最小变动价位
    listed_date     = tb.Int32Col(pos=12)  # 上市日期
    delisted_date = tb.Int32Col(pos=13)  # 退市日期
    trade_n     = tb.Int32Col(pos=14)  # 交易日数
    board       = tb.Int32Col(pos=15)  # 所属板块
    belong_index = tb.Int32Col(pos=16)  # 所属指数 hs300 1 zz500 2 zz1000 4
    underlying_symbol = tb.StringCol(17, pos=17)  # 标的证券代码
    conversion_start_date = tb.Int32Col(pos=18)   # 转换起始日
    ttl_shr   = tb.Int64Col(pos=19)  # 总股本
    a_shr_unl   = tb.Int64Col(pos=20)  # 流通股本


class DailyBar(tb.IsDescription):
    trade_date = tb.Int32Col(pos=0)
    symbol = tb.StringCol(11, pos=1)
    open = tb.Float32Col(pos=2)
    high = tb.Float32Col(pos=3)
    low = tb.Float32Col(pos=4)
    close = tb.Float32Col(pos=5)
    volume = tb.Int64Col(pos=6)
    amount = tb.Float32Col(pos=7)
    pre_close = tb.Float32Col(pos=8)
    turnrate = tb.Float32Col(pos=9)
    market_cap = tb.Float32Col(pos=10)
    market_cap_circ = tb.Float32Col(pos=11)
    sec_type = tb.Int32Col(pos=12)
    upper_limit = tb.Float32Col(pos=13)
    lower_limit = tb.Float32Col(pos=14)
    adj_factor = tb.Float32Col(pos=15)
    belong_index = tb.Int32Col(pos=17)


class MinuteBar(tb.IsDescription):
    trade_date = tb.Int32Col(pos=0)
    symbol = tb.StringCol(11, pos=1)
    open = tb.Float32Col(pos=2)
    high = tb.Float32Col(pos=3)
    low = tb.Float32Col(pos=4)
    close = tb.Float32Col(pos=5)
    volume = tb.Int64Col(pos=6)
    amount = tb.Float32Col(pos=7)
    pre_close = tb.Float32Col(pos=8)
    bob = tb.Int32Col(pos=9)
    eob = tb.Int32Col(pos=10)
    

