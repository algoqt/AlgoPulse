#include "Algo.h"
#include "StockDataManager.h"

using AshareMarketTime = agcommon::AshareMarketTime;

 std::vector<int> TWAP::allocateCumulativeQty(int qtyOfTarget
     , int numOfSlicers
     , const SecurityStaticInfo* ssinfo) {

    if (numOfSlicers <= 0) {
        return { qtyOfTarget };
    }
    std::vector<double> weights;

    for (int i = 1; i <= numOfSlicers; i++) {
        weights.push_back(static_cast<double>(i) / numOfSlicers);
    }

    std::vector<int> cumQtyOfSlicers;
    for (double w : weights) {
        cumQtyOfSlicers.push_back(static_cast<int>(w * qtyOfTarget));
    }

    if (numOfSlicers >= 2) {
        int lastSlicerQty = cumQtyOfSlicers.back() - cumQtyOfSlicers.at(numOfSlicers - 2);
        if (lastSlicerQty < ssinfo->orderInitQtySize) {
            cumQtyOfSlicers[numOfSlicers - 2] = qtyOfTarget - ssinfo->orderInitQtySize;
            for (int j = numOfSlicers - 2 - 1; j >= 0; j--) {
                if (cumQtyOfSlicers[j] > cumQtyOfSlicers.at(numOfSlicers - 2)) {
                    cumQtyOfSlicers[j] = cumQtyOfSlicers.at(numOfSlicers - 2);
                    break;
                }
            }
        }
    }
    return cumQtyOfSlicers;
}



std::vector<int> VWAP::allocateCumulativeQty(int qtyOfTarget
    , int numOfSlicers
    , const SecurityStaticInfo* ssinfo
    , boost::posix_time::ptime startTime
    , boost::posix_time::ptime endTime
    , const std::vector<double>& volCurve) 

{

    if (numOfSlicers == 1) {
        return std::vector<int>{ qtyOfTarget };
    }
    int startIndx = (int)AshareMarketTime::getDurationSinceMarketOpen(startTime) / 60;
    int endIndx = (int)std::ceil(AshareMarketTime::getDurationSinceMarketOpen(endTime) / 60);

    std::vector<double> volSample;

    if (volCurve.size() == 240) {
        volSample = std::vector<double>(volCurve.begin() + startIndx, volCurve.begin() + endIndx);
    }
    else {
        volSample = std::vector<double>(volCurveDefault.begin() + startIndx, volCurveDefault.begin() + endIndx);
    }

    std::vector<double> weights(volSample.size());

    double sumVolSample = std::accumulate(volSample.begin(), volSample.end(), 0.0);

    std::partial_sum(volSample.begin(), volSample.end(), weights.begin());

    std::transform(weights.begin(), weights.end(), weights.begin(),
        [sumVolSample](double w) { return w / sumVolSample; });

    double step = static_cast<double>(weights.size() - 1) / (numOfSlicers - 1);
    std::vector<double> interpWeights(numOfSlicers);
    for (int i = 0; i < numOfSlicers; ++i) {
        double x = i * step;
        int lowerIndex = static_cast<int>(std::floor(x));
        int upperIndex = static_cast<int>(std::ceil(x));
        if (lowerIndex == upperIndex) {
            interpWeights[i] = weights[lowerIndex];
        }
        else {
            double lowerWeight = weights[lowerIndex];
            double upperWeight = weights[upperIndex];
            double t = x - lowerIndex;
            interpWeights[i] = (1 - t) * lowerWeight + t * upperWeight;
        }
    }
    std::vector<int> cumQtyOfSlicers;
    for (double w : interpWeights) {
        cumQtyOfSlicers.push_back(static_cast<int>(w * qtyOfTarget));
    }

    if (cumQtyOfSlicers.size() != numOfSlicers) {
        SPDLOG_WARN(" vwap no match. {},{}", numOfSlicers, cumQtyOfSlicers.size());
        //numOfSlicers = cumQtyOfSlicers.size();
    }
    if (numOfSlicers >= 2) {
        int lastSlicerQty = cumQtyOfSlicers.back() - cumQtyOfSlicers.at(numOfSlicers - 2);
        auto orderInitQty = ssinfo->getOrderInitQty();
        if (lastSlicerQty < orderInitQty) {
            cumQtyOfSlicers.at(numOfSlicers - 2) = qtyOfTarget - orderInitQty;
            for (int j = numOfSlicers - 2 - 1; j >= 0; j--) {
                if (cumQtyOfSlicers[j] > cumQtyOfSlicers.at(numOfSlicers - 2)) {
                    cumQtyOfSlicers[j] = cumQtyOfSlicers.at(numOfSlicers - 2);
                    break;
                }
            }
        }
    }

    return cumQtyOfSlicers;
}

std::vector<double> VWAP::volCurveDefault = { 3.349, 1.906, 1.668, 1.608, 1.388, 1.295, 1.244, 1.114, 1.059, 1.06, 1.149, 1.027, 0.98, 0.918, 0.863, 0.902, 0.839,
                                                0.814, 0.804, 0.738, 0.864, 0.753, 0.689, 0.667, 0.67, 0.674, 0.624, 0.619, 0.603, 0.579, 0.689, 0.634, 0.586, 0.572,
                                                0.536, 0.557, 0.534, 0.52, 0.494, 0.514, 0.54, 0.497, 0.474, 0.462, 0.44, 0.47, 0.453, 0.434, 0.446, 0.445, 0.471, 0.433,
                                                0.404, 0.405, 0.407, 0.403, 0.382, 0.371, 0.381, 0.408, 0.464, 0.445, 0.393, 0.369, 0.377, 0.352, 0.371, 0.361, 0.361,
                                                0.349, 0.355, 0.352, 0.347, 0.311, 0.312, 0.325, 0.314, 0.316, 0.3, 0.307, 0.342, 0.324, 0.286, 0.292, 0.273, 0.273, 0.263,
                                                0.275, 0.275, 0.264, 0.311, 0.291, 0.295, 0.294, 0.272, 0.271, 0.262, 0.246, 0.244, 0.245, 0.264, 0.247, 0.257, 0.253, 0.26,
                                                0.273, 0.247, 0.249, 0.246, 0.232, 0.26, 0.252, 0.234, 0.226, 0.24, 0.225, 0.214, 0.22, 0.22, 0.282, 0.898, 0.301, 0.256, 0.259,
                                                0.28, 0.289, 0.302, 0.314, 0.302, 0.276, 0.299, 0.292, 0.265, 0.266, 0.264, 0.274, 0.276, 0.255, 0.259, 0.276, 0.273, 0.268,
                                                0.267, 0.28, 0.261, 0.287, 0.282, 0.265, 0.273, 0.274, 0.316, 0.27, 0.286, 0.257, 0.256, 0.265, 0.243, 0.237, 0.234, 0.228,
                                                0.242, 0.249, 0.246, 0.25, 0.241, 0.243, 0.251, 0.238, 0.25, 0.248, 0.239, 0.233, 0.223, 0.232, 0.238, 0.236, 0.229, 0.23,
                                                0.239, 0.241, 0.272, 0.292, 0.249, 0.247, 0.244, 0.26, 0.272, 0.255, 0.264, 0.241, 0.243, 0.243, 0.233, 0.246, 0.261, 0.267,
                                                0.262, 0.262, 0.261, 0.261, 0.274, 0.257, 0.244, 0.255, 0.266, 0.279, 0.274, 0.273, 0.282, 0.294, 0.365, 0.334, 0.313, 0.303,
                                                0.311, 0.308, 0.302, 0.32, 0.33, 0.323, 0.342, 0.346, 0.349, 0.349, 0.358, 0.441, 0.381, 0.41, 0.415, 0.426,
                                                0.532, 0.476, 0.53, 0.585, 0.567, 0.651, 0.714, 0.038, 0.0, 1.314 };
