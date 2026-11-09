// 原人物条件捕获的同候选结束覆盖；不证明业务许可或永久身体历史。
#pragma once
#include "actor_condition_execution_draft.hpp"
namespace hanmo::resident {
class CommonStore;
// 私有起点和一次消费状态；副本不得跨Common或结束重开后继承同次身份。
class ActorConditionCoverageBoundary {
public:
    ActorConditionCoverageBoundary()=default;
private:
    struct State;
    std::shared_ptr<State> state_;
    friend Outcome<ActorConditionCoverageBoundary> begin_actor_condition_coverage(CommonStore&);
    friend Outcome<std::vector<ActorConditionExecutionDraft>> finish_actor_condition_coverage(CommonStore&,ActorConditionCoverageBoundary&);
};
// 保存实际全体人物HP/疲劳与成员顺序，然后开启原原语捕获，不补任何初始来源。
Outcome<ActorConditionCoverageBoundary> begin_actor_condition_coverage(CommonStore&);
// 逐实录接上前像并核完整末态；失败无输出，调用者丢弃整个未发布World候选。
// 未捕获且完全恢复原值的中间直接写不在此覆盖能力内，永久来源须另行核证。
Outcome<std::vector<ActorConditionExecutionDraft>> finish_actor_condition_coverage(CommonStore&,ActorConditionCoverageBoundary&);
}
