// 原Growth写栈的临时条件绑定接收者；World必须自持并按同一候选/真实half-day重建。
#pragma once
#include "growth_condition_binding.hpp"
namespace hanmo::resident {
class GrowthConditionWriteServices {
public:
    virtual ~GrowthConditionWriteServices()=default;
    virtual const DynamicStepInput& condition_step()const=0;
    // 只接原writer全部成功后的实际绑定，不得先发布永久账或提交调用方Common保存点。
    // 失败由原调用链传播；暂存绑定与Common/Growth/Recovery必须同外层回滚。
    virtual Status accept_bound_condition(GrowthConditionBoundWrite&&)=0;
};
}
