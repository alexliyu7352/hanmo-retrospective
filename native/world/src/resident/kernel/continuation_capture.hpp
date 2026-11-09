// 真正run控制器捕获的拥有值；原字符串预留和BigNat实际block均跨异步保存保活。
#pragma once
#include "continuation_snapshot.hpp"
#include "resident/checkpoint/checkpoint_job_memory.hpp"
namespace hanmo::resident {
class CapturedContinuation {
public:
    CapturedContinuation()=default;
    CapturedContinuation(const CapturedContinuation&)=delete;
    CapturedContinuation& operator=(const CapturedContinuation&)=delete;
    CapturedContinuation(CapturedContinuation&&)noexcept=default;
    // 只转移拥有指针；Snapshot 内的分数仍可抛，绝不在 noexcept 移动中搬值。
    CapturedContinuation& operator=(CapturedContinuation&&)noexcept=default;
    const std::optional<ContinuationSnapshot>& value()const noexcept{
        static const std::optional<ContinuationSnapshot> empty;
        return state_?state_->value:empty;
    }
    bool belongs_to(const CheckpointJobBudget& job)const noexcept{return state_&&state_->identity.belongs_to(job);}
private:
    friend class ResidentWorld;
    struct State {
        // 销毁顺序是原快照、普通字符串预留、job 身份；真实对象 block 由 deleter 最后归还。
        CheckpointJobReservation identity;
        CheckpointMemoryCharge strings;
        std::optional<ContinuationSnapshot> value;
    };
    CheckpointUniquePtr<State> state_;
};
}
