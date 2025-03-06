#include "MarketDepth.h"
#include "H5DataTypes.h"


MarketDepth::MarketDepth(const Symbol_t& _symbol, const OrderTime_t& quoteTime ) 
    :symbol{ _symbol }
    , quoteTime(quoteTime) {
};

MarketDepth::MarketDepth(const h5data::Tick* tick, float _preClose, int64_t _share_circ) {

    auto exchange = (agcommon::MarketExchange)tick->exchange;

    symbol = fmt::format("{:06}.{}", tick->symbol, agcommon::getMarketExchangeStrCode(exchange));

    quoteTime = agcommon::AshareMarketTime::convert2ShanghaiTZ(tick->created_at);

    preClose = _preClose;

    amount = tick->cum_amount;
    volume = tick->cum_volume;
    open    = tick->open;
    low     = tick->low;
    high    = tick->high;
    price   = tick->price;

    change  = price - preClose;
    changeP = preClose > 0 ? change / preClose : 0;

    askPrice1 = tick->ask_price1;
    askPrice2 = tick->ask_price2;
    askPrice3 = tick->ask_price3;
    askPrice4 = tick->ask_price4;
    askPrice5 = tick->ask_price5;

    askVol1 = tick->ask_volume1;
    askVol2 = tick->ask_volume2;
    askVol3 = tick->ask_volume3;
    askVol4 = tick->ask_volume4;
    askVol5 = tick->ask_volume5;

    bidPrice1 = tick->bid_price1;
    bidPrice2 = tick->bid_price2;
    bidPrice3 = tick->bid_price3;
    bidPrice4 = tick->bid_price4;
    bidPrice5 = tick->bid_price5;

    bidVol1 = tick->bid_volume1;
    bidVol2 = tick->bid_volume2;
    bidVol3 = tick->bid_volume3;
    bidVol4 = tick->bid_volume4;
    bidVol5 = tick->bid_volume5;

    deltaAmount = tick->last_amount;

    deltaVolume = tick->last_volume;

    turnRate = _share_circ > 0 ? tick->cum_volume * 100.0 / _share_circ : 0;

}

std::vector<std::pair<double,int>> MarketDepth::getAskQuotes() const {
    std::vector<std::pair<double,int>> quotes{
        {askPrice1,askVol1},
        {askPrice2,askVol2},
        {askPrice3,askVol3},
        {askPrice4,askVol4},
        {askPrice5,askVol5}
    };
    return quotes;
}

std::vector<std::pair<double,int>> MarketDepth::getBidQuotes() const {

    std::vector<std::pair<double,int>> quotes{
        {bidPrice1,bidVol1},
        {bidPrice2,bidVol2},
        {bidPrice3,bidVol3},
        {bidPrice4,bidVol4},
        {bidPrice5,bidVol5}
    };
    return quotes;
}

std::unordered_map<double,int> MarketDepth::getPrice2VolMap(const agcommon::QuoteSide qs) const {
    if (qs == agcommon::QuoteSide::Ask) {
        std::unordered_map<double, int> price2vol{
			{askPrice1,askVol1},
			{askPrice2,askVol2},
			{askPrice3,askVol3},
			{askPrice4,askVol4},
			{askPrice5,askVol5}
		};
		return price2vol;
    }
    if (qs == agcommon::QuoteSide::Bid) {
        std::unordered_map<double, int> price2vol{
            {bidPrice1,bidVol1},
            {bidPrice2,bidVol2},
            {bidPrice3,bidVol3},
            {bidPrice4,bidVol4},
            {bidPrice5,bidVol5}
        };
        return price2vol;
    }

    std::unordered_map<double,int> price2vol{
        {bidPrice1,bidVol1},
        {bidPrice2,bidVol2},
        {bidPrice3,bidVol3},
        {bidPrice4,bidVol4},
        {bidPrice5,bidVol5},
        {askPrice1,askVol1},
        {askPrice2,askVol2},
        {askPrice3,askVol3},
        {askPrice4,askVol4},
        {askPrice5,askVol5}
    };
    return price2vol;
}

int MarketDepth::getPricezVol(const double price) const {

    auto price2vol = getPrice2VolMap(agcommon::QuoteSide::Both);
    if (price2vol.find(price) != price2vol.end()) {
		return price2vol[price];
	}
    return 0;
}
std::pair<int,double> MarketDepth::tryMathWithinPrice(agcommon::QuoteSide qs, double price,int qty) const {
    const double tryMatchPercent = 0.2;
    int     mathQty = 0;
    double  filledAmount = 0;
    int     filledQty = 0;
    if (qs == agcommon::QuoteSide::Ask) {
        auto priceVol = getAskQuotes();
        for (const auto& [p, vol] : priceVol) {
            if (p>0 and p <= price) {
                mathQty      += int(vol * tryMatchPercent);
                filledQty    += mathQty;
                filledAmount += p * mathQty;
                if (filledQty >= qty) {
                    filledAmount = filledAmount - p * (filledQty - qty);
                    return std::make_pair(qty, filledAmount);
                }
            }
            else {
                break;
            }
        }
        return std::make_pair(filledQty, filledAmount);
    }
    else if (qs == agcommon::QuoteSide::Bid) {
        auto priceVol = getBidQuotes();
        for (const auto& [p, vol] : priceVol) {
            if (p >= price and price>0) {
                mathQty      += int(vol * tryMatchPercent);
                filledQty    += mathQty;
                filledAmount += p * mathQty;
                if (filledQty >= qty) {
                    filledAmount = filledAmount - p * (filledQty - qty);
                    return std::make_pair(qty, filledAmount);
                }
            }
            else {
                break;
            }
        }
        return std::make_pair(filledQty, filledAmount);
    }
    return std::make_pair(0, 0);
}

std::map<double, int> MarketDepth::calDelta(const MarketDepth* lastMd) {

    std::map<double, int> deltaDepth;

    if (quoteTime > lastMd->quoteTime ) {  
        if (deltaVolume == 0) {
            deltaVolume = volume - lastMd->volume;
        }
        if (deltaAmount == 0) {
            deltaAmount = amount - lastMd->amount;
        }

        for (auto& [p, v] : getAskQuotes()) { 
            auto p2vs = lastMd->getPrice2VolMap(agcommon::QuoteSide::Ask);
            if (p2vs.find(p) != p2vs.end()) {
                deltaDepth[p] = v - p2vs[p];
            }
        }
        for (auto& [p, v] : getBidQuotes()) { 
            auto p2vs = lastMd->getPrice2VolMap(agcommon::QuoteSide::Bid);
            if (p2vs.find(p) != p2vs.end()) {
                deltaDepth[p] = v - p2vs[p];
            }
        }
        bsType = 0;
        if (deltaVolume > 0 && deltaAmount > 0) {

            if (deltaAmount/deltaVolume > 0.5 * (lastMd->askPrice1 + lastMd->bidPrice1)) {
                bsType = 1;
            }
            else if (deltaAmount / deltaVolume < 0.5 * (lastMd->askPrice1 + lastMd->bidPrice1)) {
                bsType = -1;
            }
            flow = bsType * deltaAmount;
        }
    }
    else {
        bsType = 0;
        deltaVolume = 0;
        deltaAmount = 0;
        flow = 0;
    }
    return deltaDepth;
}

void MarketDepth::parseMapString(std::map<std::string, std::string>& row) {

    preClose = agcommon::get_float(row["pre_close"]);

    amount  = agcommon::get_double(row["money"]);
    volume  = agcommon::get_int(row["volume"]);
    open    = agcommon::get_float(row["open"]);
    low     = agcommon::get_float(row["low"]);
    high    = agcommon::get_float(row["high"]);
    price   = agcommon::get_float(row["current"]);

    change = price - preClose;
    changeP = preClose > 0 ? change / preClose : 0;

    askPrice1 = agcommon::get_float(row["a1_p"]);
    askPrice2 = agcommon::get_float(row["a2_p"]);
    askPrice3 = agcommon::get_float(row["a3_p"]);
    askPrice4 = agcommon::get_float(row["a4_p"]);
    askPrice5 = agcommon::get_float(row["a5_p"]);
    askVol1 = agcommon::get_int(row["a1_v"]);
    askVol2 = agcommon::get_int(row["a2_v"]);
    askVol3 = agcommon::get_int(row["a3_v"]);
    askVol4 = agcommon::get_int(row["a4_v"]);
    askVol5 = agcommon::get_int(row["a5_v"]);

    bidPrice1 = agcommon::get_float(row["b1_p"]);
    bidPrice2 = agcommon::get_float(row["b2_p"]);
    bidPrice3 = agcommon::get_float(row["b3_p"]);
    bidPrice4 = agcommon::get_float(row["b4_p"]);
    bidPrice5 = agcommon::get_float(row["b5_p"]);
    bidVol1 = agcommon::get_int(row["b1_v"]);
    bidVol2 = agcommon::get_int(row["b2_v"]);
    bidVol3 = agcommon::get_int(row["b3_v"]);
    bidVol4 = agcommon::get_int(row["b4_v"]);
    bidVol5 = agcommon::get_int(row["b5_v"]);

    auto tmp = agcommon::parseDateTimeStr(row["quoteTime"]);
    if (tmp)
        quoteTime = *tmp;
    else
        quoteTime = agcommon::now();
}

std::string MarketDepth::to_string() const {

    constexpr auto formatPrice =   [](double price) { return fmt::format("{:.3f}", price); };
    constexpr auto formatAmount = [](double amount) { return fmt::format("{:.1f}", amount); };

    auto formattedQuoteTime = agcommon::getDateTimeStr(quoteTime);

    return std::format(
        "quoteTime:{}, symbol:{}, price:{}, tradeVol:{}, bidPrice1:{}, bidVol1:{}, askPrice1:{}, askVol1:{}, amount:{}, volume:{}",
        formattedQuoteTime,
        symbol,
        formatPrice(price), 
        deltaVolume,
        formatPrice(bidPrice1),
        bidVol1,
        formatPrice(askPrice1),
        askVol1,
        formatAmount(amount),
        volume                   
    );
}

std::shared_ptr<AlgoMsg::MsgMarketDepth> MarketDepth::encode2AlgoMessage(uint64_t subscribeKey) const{
    
    std::shared_ptr<AlgoMsg::MsgMarketDepth> msg = std::make_shared<AlgoMsg::MsgMarketDepth>();
    msg->set_symbol(symbol);
    msg->set_quote_time(agcommon::getDateTimeInt(quoteTime));
    msg->set_price(price);
    msg->set_amount(amount);
    msg->set_volume(volume);
    msg->set_delta_volume(deltaVolume);
    msg->set_delta_amount(deltaAmount);

    msg->set_pre_close(preClose);
    msg->set_open(open);
    msg->set_low(low);
    msg->set_high(high);
    msg->set_bid_price1(bidPrice1);
    msg->set_bid_vol1(bidVol1);
    msg->set_ask_price1(askPrice1);
    msg->set_ask_vol1(askVol1);
    msg->set_bid_price2(bidPrice2);
    msg->set_bid_vol2(bidVol2);
    msg->set_ask_price2(askPrice2);
    msg->set_ask_vol2(askVol2);
    msg->set_bid_price3(bidPrice3);
    msg->set_bid_vol3(bidVol3);
    msg->set_ask_price3(askPrice3);
    msg->set_ask_vol3(askVol3);
    msg->set_bid_price4(bidPrice4);
    msg->set_bid_vol4(bidVol4);
    msg->set_ask_price4(askPrice4);
    msg->set_ask_vol4(askVol4);
    msg->set_bid_price5(bidPrice5);
    msg->set_bid_vol5(bidVol5);
    msg->set_ask_price5(askPrice5);
    msg->set_ask_vol5(askVol5);
    msg->set_turn_rate(turnRate);
    msg->set_subscribe_key(subscribeKey);

    return msg;
}

//
//MarketDepth::MarketDepth(const MarketDepth& other) {
//
//    SPDLOG_DEBUG("copy_tag,{},{}", other.symbol,agcommon::getDateTimeStr(quoteTime));
//
//    symbol = other.symbol;
//    quoteTime = other.quoteTime;
//    preClose = other.preClose;
//    amount = other.amount;
//    volume = other.volume;
//    open = other.open;
//    low = other.low;
//    high = other.high;
//    price = other.price;
//    change = other.change;
//    changeP = other.changeP;
//
//    askPrice1 = other.askPrice1;
//    askPrice2 = other.askPrice2;
//    askPrice3 = other.askPrice3;
//    askPrice4 = other.askPrice4;
//    askPrice5 = other.askPrice5;
//    askVol1 = other.askVol1;
//    askVol2 = other.askVol2;
//    askVol3 = other.askVol3;
//    askVol4 = other.askVol4;
//    askVol5 = other.askVol5;
//
//    bidPrice1 = other.bidPrice1;
//    bidPrice2 = other.bidPrice2;
//    bidPrice3 = other.bidPrice3;
//    bidPrice4 = other.bidPrice4;
//    bidPrice5 = other.bidPrice5;
//
//    bidVol1 = other.bidVol1;
//    bidVol2 = other.bidVol2;
//    bidVol3 = other.bidVol3;
//    bidVol4 = other.bidVol4;
//    bidVol5 = other.bidVol5;
//
//    deltaAmount = other.deltaAmount;
//    deltaVolume = other.deltaVolume;
//    turnRate    = other.turnRate;
//    //m_refcount  = 0;
//}
//
//MarketDepth& MarketDepth::operator=(const MarketDepth& other) {
//
//    if (this == &other) {
//        return *this;
//    }
//
//    symbol = other.symbol;
//    quoteTime = other.quoteTime;
//    preClose = other.preClose;
//    amount = other.amount;
//    volume = other.volume;
//    open = other.open;
//    low = other.low;
//    high = other.high;
//    price = other.price;
//    change = other.change;
//    changeP = other.changeP;
//
//    askPrice1 = other.askPrice1;
//    askPrice2 = other.askPrice2;
//    askPrice3 = other.askPrice3;
//    askPrice4 = other.askPrice4;
//    askPrice5 = other.askPrice5;
//    askVol1 = other.askVol1;
//    askVol2 = other.askVol2;
//    askVol3 = other.askVol3;
//    askVol4 = other.askVol4;
//    askVol5 = other.askVol5;
//
//    bidPrice1 = other.bidPrice1;
//    bidPrice2 = other.bidPrice2;
//    bidPrice3 = other.bidPrice3;
//    bidPrice4 = other.bidPrice4;
//    bidPrice5 = other.bidPrice5;
//
//    bidVol1 = other.bidVol1;
//    bidVol2 = other.bidVol2;
//    bidVol3 = other.bidVol3;
//    bidVol4 = other.bidVol4;
//    bidVol5 = other.bidVol5;
//
//    deltaAmount = other.deltaAmount;
//    deltaVolume = other.deltaVolume;
//    turnRate    = other.turnRate;
//
//    return *this;
//}