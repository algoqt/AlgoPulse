from tableDefs import *
from typing import Dict, List, Tuple
from gm.api import *
import pandas as pd
from config import *

def download_trading_dates_by_year(start_year, end_year):
    beg_dt = start_year * 10000 + 101
    end_dt = end_year * 10000 + 1231
    with tb.open_file(TRADE_CALENDAR_FILE, mode='a', title="Trade Calendar Info") as h5file:
        if  h5file.__contains__("/TradeCalendar"):
            table = h5file.get_node("/TradeCalendar")
        else:
            table = h5file.create_table("/", 'TradeCalendar', TradeCalendar)
            table.cols.date.create_index()
        condition = f'((date >= {beg_dt}) & (date <= {end_dt}))'
        existing_records = [row.nrow for row in table.where(condition)]
        if len(existing_records)>0:
            mainLogger.info(f"[overwrite]{beg_dt}~{end_dt} already exists:{len(existing_records)}")
            for row_nrow in existing_records:
                table.remove_rows(row_nrow)

        dates = get_trading_dates_by_year(exchange='SHSE', start_year=start_year, end_year=end_year)
        row = table.row
        for idx,date_row in dates.iterrows():
            row['date']             = int(date_row['date'].replace('-',''))
            row['trade_date']       = int(date_row['trade_date'].replace('-','')) if date_row['trade_date'] else 0
            row['next_trade_date']  = int(date_row['next_trade_date'].replace('-',''))
            row['pre_trade_date']   = int(date_row['pre_trade_date'].replace('-',''))
            row.append()
        mainLogger.info(f"[insert]{beg_dt}~{end_dt} total:{len(dates)}")
        table.flush()
        

def downloadStaticInfo(symbols:List[str],start_date:str,end_date:str,overwrite=False):
    beg_dt = int(start_date.replace("-",""))
    end_dt = int(end_date.replace("-",""))
    pre_date = get_previous_n_trading_dates("SZSE", date=start_date, n=1)[0]

    with tb.open_file(STATICINFO_FILE, mode='a', title="A Static Daily Info") as h5file:
        node_key = "I{}".format(end_dt)
        if  h5file.__contains__("/"+node_key):
            if overwrite:
                h5file.remove_node("/"+node_key, recursive=True)
            else:
                return
        table = h5file.create_table("/", node_key, StockInfo, title=node_key)
        #hist_ = get_symbols(sec_type1, sec_type2=None, exchanges=None, symbols=None, skip_suspended=True, skip_st=True, trade_date=None, df=False)
        hist_ = get_history_instruments(symbols, fields=None ,start_date=start_date, end_date=end_date,df=True)
        hist_["trade_date"]=hist_["trade_date"].apply(lambda x:int(x.strftime("%Y%m%d")))

        basic = stk_get_daily_basic_pt(symbols,fields='turnrate,ttl_shr,a_shr_unl',trade_date=pre_date,df=True)

        basic["trade_date"]= int(start_date.replace("-",""))  # basic["trade_date"].apply(lambda x:int(x.replace("-","")))
        hist_ = pd.merge(hist_,basic,on=["trade_date","symbol"],how="left")
        nozeros =     len(hist_[hist_["ttl_shr"]>0])
        nozeros_unl = len(hist_[hist_["a_shr_unl"]>0])
        mainLogger.warning(f"Total {len(hist_)} stocks, {nozeros} with ttl_shr>0, {nozeros_unl} with a_shr_unl>0")
        #print(hist_.head())
        with timeCount("StockInfo"):

            hs300 = stk_get_index_constituents("SHSE.000300",trade_date=start_date)[["symbol",'weight']]
            hs300 = {x["symbol"]:x["weight"] for _,x in hs300.iterrows()}
            zz500 = stk_get_index_constituents(index="SHSE.000905",trade_date=start_date)[["symbol",'weight']]
            zz500 = {x["symbol"]:x["weight"] for _,x in zz500.iterrows()}
            zz1000 = stk_get_index_constituents(index="SHSE.000852",trade_date=start_date)[["symbol",'weight']]
            zz1000 = {x["symbol"]:x["weight"] for _,x in zz1000.iterrows()}

        row = table.row
        #diff =  set(symbols) - set([item["symbol"] for item in hist_])
        # if len(diff) > 0:
        #     print(diff)
    # with tb.open_file(filename, mode='w', title="A Stock Daily Info") as h5file:
        for idx,item in hist_.iterrows():  # Example of adding 10 rows of data

            row['trade_date'] =  item["trade_date"]
            row['symbol']     =  item["symbol"]
            row['sec_level']  =  item["sec_level"]
            row['sec_type']   =  item["sec_type"]
            row['exchange']   =  item["exchange"]
            row['sec_id']     =  item["sec_id"]
            row['sec_name']   =  item["sec_name"].encode('utf-8')

            row['pre_close']   =  item["pre_close"]
            row['upper_limit'] =  item["upper_limit"]
            row['lower_limit'] =  item["lower_limit"]
            row['adj_factor']  =  item["adj_factor"]
            row['is_suspended']  =  int(item["is_suspended"])
            row['price_tick']    =  item["price_tick"]
            row['listed_date']   =  (int(item["listed_date"].strftime("%Y%m%d"))   if item["listed_date"] else 19910101)
            row['delisted_date'] =  (int(item["delisted_date"].strftime("%Y%m%d")) if item["delisted_date"] else 29910101)
            row['trade_n']       =  item["trade_n"]
            row['board']         =  item["board"]
            row["underlying_symbol"]    =  item["underlying_symbol"]
            row["conversion_start_date"] =  (int(item["conversion_start_date"].strftime("%Y%m%d")) if pd.notna(item["conversion_start_date"]) else 0)
            row["ttl_shr"]  =   (int(item["ttl_shr"]) if pd.notna(item["ttl_shr"]) else 0)
            row["a_shr_unl"] =  (int(item["a_shr_unl"]) if pd.notna(item["a_shr_unl"]) else 0)
            belong_index=0
            if item["symbol"] in hs300:
                belong_index=1
            elif item["symbol"] in zz500:
                belong_index=2
            elif item["symbol"] in zz1000:
                belong_index=4
            #if belong_index>0:
                #print(item["symbol"],belong_index)
            row["belong_index"]  =  belong_index
            if row['delisted_date']==29910101  and (item["symbol"]=="" or item["pre_close"]==0):
                print(item["symbol"],item["pre_close"])
                continue
            row.append()
        table.flush()
    mainLogger.info(f"insert static Info {len(hist_)}")


