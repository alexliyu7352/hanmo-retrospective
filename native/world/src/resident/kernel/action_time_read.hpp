// Common只委托这一条时间谓词；完整身份/占用/资源校验仍由Common原validator负责。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
class ActionTimeValidationRead {
public:
    virtual ~ActionTimeValidationRead()=default;
    virtual Status validate_action_time(const ActionCoreView&) const=0;
};
}
