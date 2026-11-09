// 同一次人物条件捕获内的保存点只读前界；不是业务、HP或advance成功许可。
#pragma once
#include "resident/model/common_operations.hpp"
namespace hanmo::resident {
class CommonStore;
struct ActorConditionCaptureIdentity;
// 只能由原Common读出；持有原捕获身份防止结束重开或候选移动后复用。
class ActorConditionSavepointBoundary {
    friend class CommonStore;
    const CommonStore* common_=nullptr;
    std::shared_ptr<const ActorConditionCaptureIdentity> capture_;
    Integer issued_sequence_=0;
    std::optional<Integer> active_token_;
};
}
