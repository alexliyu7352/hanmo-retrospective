// 同一实际人物捕获中的局部读集寿命证明；不授予业务成功、来源绑定或保存点刷新权限。
#pragma once
#include "resident/kernel/source_ref.hpp"
#include <memory>
#include <string_view>
#include <vector>

namespace hanmo::resident {
class CommonStore;
struct ActorConditionCaptureIdentity;

// 仅工厂可建立证明；拥有复制键和原捕获身份，任何已观察写入都不可重置或重新武装。
class CommonCaptureReadSetProof final : public CheckpointAllocatedObject {
public:
    // 需活动人物捕获和无重复的有效键；来源还须匹配当前真实共同事实头。
    static Outcome<std::unique_ptr<CommonCaptureReadSetProof>> capture(
        const CommonStore&, const std::vector<ResourceKey>&,
        const std::vector<SourceRef>& = {});
    // 仅相同共同对象及相同活动捕获可验证；失败保留人物、行动或来源失效原因。
    Status validate(const CommonStore&) const;
    // 释放观察状态；弱注册不延长证明寿命，也不形成捕获拥有环。
    ~CommonCaptureReadSetProof();
    CommonCaptureReadSetProof(const CommonCaptureReadSetProof&) = delete;
    CommonCaptureReadSetProof& operator=(const CommonCaptureReadSetProof&) = delete;
    CommonCaptureReadSetProof(CommonCaptureReadSetProof&&) = delete;
    CommonCaptureReadSetProof& operator=(CommonCaptureReadSetProof&&) = delete;

private:
    friend struct ActorConditionCaptureIdentity;
    struct State;
    explicit CommonCaptureReadSetProof(std::shared_ptr<State>) noexcept;
    std::shared_ptr<State> state_;
};

// 原来源写入/移除者在改头前调用；仅置脏匹配读集，不分配内存或建立任何新证明。
void invalidate_common_capture_source(const CommonStore&, std::string_view source_id) noexcept;
}
