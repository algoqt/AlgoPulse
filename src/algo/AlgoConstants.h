#pragma once
#include <cstdint>

namespace AlgoConstants {

    static inline double Max_Take_Impact_VolRate    = 0.5;
    static inline double Default_CoverRate          = 0.25;

    static inline int32_t Allow_Make_Condition_OrderCnt  = 10;

    static inline double Allow_Make_Threshold_FilledRate = 60.0;
    static inline double Allow_Make_Threshold_CancelRate = 40.0;

    static inline double Allow_Make_Threshold_FilledRate_LowPrice = 30.0;
    static inline double Allow_Make_Threshold_CancelRate_LowPrice = 50.0;

    static inline int32_t Should_Take_Condition_OrderCnt = 6;

    static inline double Should_Take_Threshold_FilledRate = 50.0;
    static inline double Should_Take_Condition_TimeProgress = 0.15;

    static inline double Should_Take_Threshold_FilledRate_LowPrice = 30.0;
    static inline double Should_Take_Condition_TimeProgress_LowPrice = 0.3;

    int init() ;
}