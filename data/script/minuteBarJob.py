from tableDefs import *
from functools import partial
from typing import Dict, List, Tuple
from gm.api import *
import pandas as pd
import concurrent.futures
from config import *


def _downloadMinuteBar(symbols,start_time,end_time):

    dt = start_time.split(" ")[0]
    bars = history(symbols, frequency='60s',start_time=start_time,end_time=end_time,fill_missing='last',adjust= ADJUST_NONE
                    ,skip_suspended=False,df=True)
    bars["trade_date"]=bars['bob'].apply(lambda x:int(x.strftime("%Y%m%d")))
    bars["bob"]=bars['bob'].apply(lambda x:int(x.strftime("%H%M%S")))
    bars["eob"]=bars['eob'].apply(lambda x:int(x.strftime("%H%M%S")))
    del bars["pre_close"]
    mainLogger.info(f"{dt} symbol minBar {len(symbols)},{len(bars)}")
    if len(bars)==0:
        return dt,[]
    return dt,bars

###########################################

def downloadMinuteBar(dt:str,executor=None):
    start_time=dt+" 09:30:00"
    end_time  =dt+" 15:00:00"
    filters = tb.Filters(complevel=9, complib='zlib')
    dtstr = dt.replace("-","")
    with tb.open_file(STATICINFO_FILE, mode='r') as store:
        node_key = "/I{}".format(dtstr)
        ssinfos = store.get_node(node_key).read()
        data=pd.DataFrame(ssinfos)
        data["symbol"]=data["symbol"].apply(lambda x:x.decode())
        symbols = data["symbol"].tolist()

    chunk_size = 135
    arrays = [symbols[i:i + chunk_size] for i in range(0, len(symbols), chunk_size)] 
    my_function_partial = partial(_downloadMinuteBar,start_time=start_time, end_time=end_time)
    if executor:
        bars = executor.map(my_function_partial,arrays)
    else:
        with concurrent.futures.ThreadPoolExecutor(max_workers=16) as executor:
            bars = executor.map(my_function_partial,arrays)
    bars = [ bar for _,bar in bars if len(bar)>0]

    bars = pd.concat(bars)
    bars = bars.merge(data,how="left",on=["symbol","trade_date"])

    with tb.open_file(MINUTEBAR_FILE, mode='a', title="Minute Bar") as h5file:
        node_key = "M{}".format(dtstr)
        if  h5file.__contains__("/"+node_key):
            h5file.remove_node("/"+node_key, recursive=True)
            mainLogger.info("remove node: /{}".format(node_key))

        table = h5file.create_table("/", node_key, MinuteBar, title=node_key,filters =filters)
        row = table.row
        for idx,item in bars.iterrows():
            row['trade_date'] =  item["trade_date"]
            row['symbol']     =  item["symbol"]
            row['open']      =  item["open"]
            row['high']      =  item["high"]
            row['low']       =  item["low"]
            row['close']     =  item["close"]
            row['volume']    =  item["volume"]
            row['amount']    =  item["amount"]
            row['pre_close']   =  item["pre_close"]
            row["bob"] = item["bob"]
            row["eob"] = item["eob"]
            row.append()
        table.flush()

    mainLogger.info(f"{dt} symbol minBar {len(symbols)},{len(bars)}")


def downloadDailyBar(symbols:List[str],start_date:str,end_date:str,overwrite=False):
    beg_dt = int(end_date.replace("-",""))
    end_dt = int(end_date.replace("-",""))

    with tb.open_file(STATICINFO_FILE, mode='r') as store,tb.open_file(DAILYBAR_FILE, mode='a', title="Daily Bar") as h5file:
        node_key = "/I{}".format(beg_dt)
        ssinfos = store.get_node(node_key).read()
        data=pd.DataFrame(ssinfos)
        data["symbol"]=data["symbol"].apply(lambda x:x.decode())
        symbols = data["symbol"].tolist()

        node_key = "D{}".format(beg_dt)
        if  h5file.__contains__("/"+node_key):
            h5file.remove_node("/"+node_key, recursive=True)
        table = h5file.create_table("/", node_key, DailyBar, title=node_key)

        bars = history(symbols, frequency='1d',start_time=start_date,end_time=end_date,fill_missing='last',fields=None
                    ,skip_suspended=False,df=True)
        if len(bars)==0:
            mainLogger.info(f"{start_date} {end_date} query bars size :{len(bars)}")
            return
        
        bars["trade_date"]=bars['bob'].apply(lambda x:int(x.strftime("%Y%m%d")))
        del bars["pre_close"]

        # basic = stk_get_daily_basic_pt(symbols,fields='turnrate,ttl_shr,a_shr_unl',trade_date=start_date,df=True)
        # basic["trade_date"]=basic["trade_date"].apply(lambda x:int(x.replace("-","")))
        # result=bars.merge(basic,how='left',on=["symbol","trade_date"])
        all_res = bars.merge(data,on=["trade_date","symbol"])
        all_res['market_cap'] = all_res['close'] * all_res['ttl_shr']/1e8
        all_res['market_cap_circ'] = all_res['close'] * all_res['a_shr_unl']/1e8
        all_res['turnrate'] = all_res.apply(lambda row:row['volume']*100.0 / row['a_shr_unl'] if row['a_shr_unl']>0 else 0, axis=1)
        nozeros = all_res[all_res['turnrate']>0].shape[0]
        mainLogger.info(f"Total {len(all_res)} stocks, {nozeros} with turnrate>0")

        df = all_res[[ 'trade_date','symbol', 'open', 'high', 'low', 'close', 'volume',
            'amount', 'pre_close', 'turnrate',  'market_cap', 'market_cap_circ', 'sec_type',
            'upper_limit', 'lower_limit','adj_factor','is_suspended', 'belong_index']]
        df
        row = table.row
        for idx,item in df.iterrows():
            row['trade_date'] =  item["trade_date"]
            row['symbol']     =  item["symbol"]
            row['open']      =  item["open"]
            row['high']      =  item["high"]
            row['low']       =  item["low"]
            row['close']     =  item["close"]
            row['volume']    =  item["volume"]
            row['amount']    =  item["amount"]
            row['pre_close']   =  item["pre_close"]
            row['upper_limit']  =  item["upper_limit"]
            row['lower_limit']  =  item["lower_limit"]
            row['turnrate']     =  item["turnrate"]
            row['market_cap']    =  item["market_cap"]
            row['market_cap_circ']    =  item["market_cap_circ"]
            row['adj_factor']  =  item["adj_factor"]

            row['sec_type']     =  item["sec_type"]
            row['belong_index']  =  item["belong_index"]

            row.append()
        table.flush()
    mainLogger.info(f"insert DailyBar Info {len(df)}")


