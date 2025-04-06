#include "AlgoConstants.h"
#include "Configs.h"

namespace AlgoConstants {
    
    template <typename T>
    void logVar(const std::string& name, const T& value) {
        SPDLOG_INFO("AlgoConstants::{}={}", name, value);
    }

    inline void logVar(const std::string& name, const double& value) {
        SPDLOG_INFO("AlgoConstants::{}={:.1f}", name, value);
    }

    #define INIT_VAR(var_name) \
        AlgoConstants::var_name = config.getConfigOrDefault("Category_ALGO", #var_name, AlgoConstants::var_name); \
        logVar(#var_name, AlgoConstants::var_name)

    int init() {

        auto& config = agcommon::Configs::getConfigs();

        INIT_VAR(Max_Take_Impact_VolRate);
        INIT_VAR(Default_Take_Confident_CoverRate);

        INIT_VAR(Allow_Make_Condition_OrderCnt);
        INIT_VAR(Allow_Make_Threshold_CancelRate);
        INIT_VAR(Allow_Make_Threshold_FilledRate);

        INIT_VAR(Maker_Price_Threshold_FilledRate);

        INIT_VAR(Allow_Make_Threshold_CancelRate_LowPrice);
        INIT_VAR(Allow_Make_Threshold_FilledRate_LowPrice);

        INIT_VAR(Should_Take_Condition_OrderCnt);
        INIT_VAR(Should_Take_Condition_TimeProgress);
        INIT_VAR(Should_Take_Threshold_FilledRate);

        INIT_VAR(Should_Take_Condition_TimeProgress_LowPrice);
        INIT_VAR(Should_Take_Threshold_FilledRate_LowPrice);

        INIT_VAR(Short_Duration_Seconds);
        INIT_VAR(LowPrice_Threshold);

        SPDLOG_INFO("init AlgoConstants done.");
        return 0;
    }

}