#pragma once

#include <map>
#include "typedefs.h"
#include "common.h"
#include "KeepAlive.h"
#include <boost/unordered_map.hpp>
#include "H5DataTypes.h"

namespace h5data {
    struct Tick;
}

class MarketDepth;

using UnorderMarketDepthMap    = boost::unordered::unordered_map<Symbol_t, MarketDepth>;

using MarketDepthKeepAlivePtr  = boost::intrusive_ptr<MarketDepth>;

using UnorderMarketDepthPtrMap = boost::unordered::unordered_map<Symbol_t, MarketDepthKeepAlivePtr>;

using UnorderMarketDepthRawPtrMap = boost::unordered::unordered_map<Symbol_t, MarketDepth*>;

class MarketDepth: public KeepAlivePool<MarketDepth> {

public:
    Symbol_t symbol                 {};

    double price    {0};
    double preClose {0};
    double open     {0};
    double high     {0};
    double low      {0};

    double   change     {0};
    double   changeP    {0};

    uint64_t volume     {0};
    double  amount     {0};
    double   avgPrice   {0};

    double   bidPrice1    {0};
    int     bidVol1      {0};

    double   bidPrice2    {0};
    int     bidVol2      {0};
    double   bidPrice3    {0};
    int     bidVol3      {0};
    double   bidPrice4    {0};
    int     bidVol4      {0};
    double   bidPrice5    {0};
    int     bidVol5      {0};

    double   askPrice1    {0};
    int     askVol1      {0};
    double   askPrice2    {0};
    int     askVol2      {0};
    double   askPrice3    {0};
    int     askVol3      {0};
    double   askPrice4    {0};
    int     askVol4      {0};
    double   askPrice5    {0};
    int     askVol5      {0};
    double   turnRate     { 0.0 };

    OrderTime_t quoteTime   { boost::posix_time::min_date_time};

    int     bsType          {0};

    int     deltaVolume     {0};

    double  deltaAmount     {0};

    double  flow            {0};


protected:


public:

    MarketDepth()  = default;

    ~MarketDepth() = default;

    //MarketDepth(const MarketDepth&);

    //MarketDepth& operator=(const MarketDepth&);

    MarketDepth(const Symbol_t& _symbol,const OrderTime_t& quoteTime = boost::posix_time::min_date_time );

    MarketDepth(const h5data::Tick* tick, float _preClose, int _share_circ = 0);

    std::vector<std::pair<double,int>> getAskQuotes() const;

    std::vector<std::pair<double,int>> getBidQuotes() const;

    std::unordered_map<double,int> getPrice2VolMap(const agcommon::QuoteSide qs) const;

    std::pair<int, double> tryMathWithinPrice(agcommon::QuoteSide qs, double price, int qty) const;

    int getPricezVol(const double price) const;

    inline double getMidPrice() const {
        if (askPrice1 > 0 and bidPrice1 > 0)
            return (askPrice1 + bidPrice1) / 2;
        return std::max(askPrice1, bidPrice1);
    }

    inline agcommon::MarketExchange getMarketExchange() const { 
        if (symbol.ends_with("SZ"))
            return agcommon::MarketExchange::SZSE;
        if (symbol.ends_with("SH"))
            return agcommon::MarketExchange::SHSE;
        return agcommon::MarketExchange::unDefined;
    }

    std::map<double, int> calDelta(const MarketDepth* lastMd);

    void parseMapString(std::map<std::string, std::string>& row);

    std::string to_string() const;

    std::shared_ptr<AlgoMsg::MsgMarketDepth> encode2AlgoMessage(uint64_t subscribeKey = 0) const;
};
