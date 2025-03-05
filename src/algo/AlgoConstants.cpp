#include "AlgoConstants.h"
#include "Configs.h"

namespace AlgoConstants {
    

    int init() {

        auto& config = agcommon::Configs::getConfigs();

        AlgoConstants::Max_Take_Impact_VolRate = config.getConfigOrDefault("Category_ALGO","Max_Take_Impact_VolRate", 0.5);
        AlgoConstants::Default_CoverRate       = config.getConfigOrDefault("Category_ALGO","Default_CoverRate", 0.25);

        AlgoConstants::Allow_Make_Condition_OrderCnt = config.getConfigOrDefault("Category_ALGO"
            , "Allow_Make_Condition_OrderCnt"
            , 10);

        AlgoConstants::Allow_Make_Threshold_CancelRate = config.getConfigOrDefault("Category_ALGO"
            , "Allow_Make_Threshold_CancelRate"
            , 40.0);

        AlgoConstants::Allow_Make_Threshold_FilledRate = config.getConfigOrDefault("Category_ALGO"
            , "Allow_Make_Threshold_FilledRate"
            , 60.0);

        AlgoConstants::Allow_Make_Threshold_CancelRate_LowPrice = config.getConfigOrDefault("Category_ALGO"
            , "Allow_Make_Threshold_CancelRate_LowPrice"
            , 50.0);

        AlgoConstants::Allow_Make_Threshold_FilledRate_LowPrice = config.getConfigOrDefault("Category_ALGO"
            ,"Allow_Make_Threshold_FilledRate_LowPrice"
            , 30.0);

        AlgoConstants::Should_Take_Condition_OrderCnt = config.getConfigOrDefault("Category_ALGO"
            ,"Should_Take_Condition_OrderCnt"
            , 6);

        AlgoConstants::Should_Take_Condition_TimeProgress = config.getConfigOrDefault("Category_ALGO"
            ,"Should_Take_Condition_TimeProgress"
            , 0.15);

        AlgoConstants::Should_Take_Threshold_FilledRate = config.getConfigOrDefault("Category_ALGO"
            ,"Should_Take_Threshold_FilledRate"
            , 50.0);

        AlgoConstants::Should_Take_Condition_TimeProgress_LowPrice = config.getConfigOrDefault("Category_ALGO"
            ,"Should_Take_Condition_TimeProgress_LowPrice"
            , 0.3);

        AlgoConstants::Should_Take_Threshold_FilledRate_LowPrice = config.getConfigOrDefault("Category_ALGO"
            ,"Should_Take_Threshold_FilledRate_LowPrice"
            ,30.0);

        SPDLOG_INFO("init AlgoConstants done.");
        return 0;
    }

}

auto tmp = AlgoConstants::init();