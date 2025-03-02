from tableDefs import *
from typing import Dict, List, Tuple
from gm.api import *
import pandas as pd
from multiprocessing import Pool, cpu_count
from config import *
import numpy as np

def initialize_process():
    from gm.api import history,history_n,ADJUST_PREV,set_token
    set_token(token_id)

def queryTicks(symbol,start_time,end_time):

    try:
        ticks = history(symbol, frequency='tick',start_time=start_time,end_time=end_time
                    ,fields=['symbol','open', 'high', 'low', 'price', 'quotes', 'cum_volume','cum_amount', 'iopv', 'last_amount', 'last_volume', 'created_at']
                    ,df=False) 
    except Exception as e:
        mainLogger.error(f"queryTicks error:{symbol},{start_time},{end_time},{e}")
        return symbol,[]

    mainLogger.info(f"queryTicks:{symbol},{start_time},{end_time},{len(ticks)}")
    return symbol,ticks

def downloadTicks(context,symbols,start_time,end_time):
    
    cpu_counts = cpu_count()
    try:
        with Pool(cpu_counts,initializer=initialize_process) as pool:
            for symbol in symbols:
                _ = pool.apply_async(queryTicks,(symbol,start_time,end_time,),callback=lambda tickData:store2H5(context,tickData)
                                     ,error_callback=lambda e:mainLogger.error(f"queryTicks error:{e}"))
            pool.close()
            pool.join()
            mainLogger.info("pool join done.")
    except Exception as e:
        mainLogger.error(f"Ctrl+C detected. Terminating pool...,{e}")
        pool.terminate()
        pool.join()
        mainLogger.error("Pool terminated.")
    finally:
        mainLogger.info(f"{context.now.strftime('%Y-%m-%d')},tick done")


def store2H5(context,symbol_ticks:Tuple[str,List[dict]]):
    symbol,ticks = symbol_ticks
    if len(ticks)==0:
        return
    filters = tb.Filters(complevel=9, complib='zlib')
    for dt in context.dts:
        dtstr = dt.replace("-","")
        _dt = datetime.datetime.strptime(dt,"%Y-%m-%d").date()
        result = [tick for tick in ticks if tick['created_at'].date()==_dt]
        _tick_key = "/" + symbol.replace(".","")
        if len(result)>0:
            with tb.open_file(TICK_FILE_PATH_PATTERN.format(dtstr=dtstr), mode='a') as h5file:
                if h5file.__contains__(_tick_key):
                    h5file.remove_node(_tick_key, recursive=True)
                    #continue
                table = h5file.create_table("/", symbol.replace(".",""), Tick, "TickData",filters =filters)
                t = table.row
                for tick in result:
                    #mainLogger.info(f"{tick['symbol']},tick:{tick.keys()}")
                    t["symbol"]   = int(tick['symbol'][-6:])
                    t["exchange"] = 0 if tick['symbol'].startswith("SZSE") else 1
                    t["open"]   = tick['open']
                    t["high"]   = tick['high']
                    t["low"]    = tick['low']
                    t["price"]  = tick['price']
                    if tick.get("quotes",None) is not None:
                        t["bid_price1"]  = tick["quotes"][0]["bid_p"]
                        t["bid_volume1"] = tick["quotes"][0]["bid_v"]
                        t["ask_price1"]  = tick["quotes"][0]["ask_p"]
                        t["ask_volume1"] = tick["quotes"][0]["ask_v"]
                        t["bid_price2"]  = tick["quotes"][1]["bid_p"]
                        t["bid_volume2"] = tick["quotes"][1]["bid_v"]
                        t["ask_price2"]  = tick["quotes"][1]["ask_p"]
                        t["ask_volume2"] = tick["quotes"][1]["ask_v"]
                        t["bid_price3"]  = tick["quotes"][2]["bid_p"]
                        t["bid_volume3"] = tick["quotes"][2]["bid_v"]
                        t["ask_price3"]  = tick["quotes"][2]["ask_p"]
                        t["ask_volume3"] = tick["quotes"][2]["ask_v"]
                        t["bid_price4"]  = tick["quotes"][3]["bid_p"]
                        t["bid_volume4"] = tick["quotes"][3]["bid_v"]
                        t["ask_price4"]  = tick["quotes"][3]["ask_p"]
                        t["ask_volume4"] = tick["quotes"][3]["ask_v"]
                        t["bid_price5"]  = tick["quotes"][4]["bid_p"]
                        t["bid_volume5"] = tick["quotes"][4]["bid_v"]
                        t["ask_price5"]  = tick["quotes"][4]["ask_p"]
                        t["ask_volume5"] = tick["quotes"][4]["ask_v"]
                    t["cum_volume"]  = tick['cum_volume']
                    t["cum_amount"]  = tick['cum_amount']
                    t["last_amount"] = tick['last_amount']
                    t["last_volume"] = tick['last_volume']
                    t["created_at"]  = np.ceil(tick['created_at'].timestamp())
                    t.append()
                #table.flush()
                mainLogger.info(f"{symbol},{dt},{len(result)}")
        else:
            mainLogger.info(f"{symbol},{dt},0")
