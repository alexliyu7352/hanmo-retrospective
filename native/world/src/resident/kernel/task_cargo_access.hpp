// 路遇货损唯一可信读取器；实际战斗、路遇、目录与人物属于同一候选。
#pragma once
#include "resident/domains/tasks/task_model.hpp"
#include "resident/domains/personal/personal_model.hpp"
namespace hanmo::resident {
class TaskCargoAccess final : public TaskCargoSourceRead {
public:
    TaskCargoAccess(const CommonStore&,const PersonalState&,const PersonalCatalog&,const InteractionState&);
    Outcome<std::optional<TaskCargoBattleSource>> task_cargo_source(const StableId& battle_id) const override;
    std::vector<StableId> task_cargo_sources() const override;
private:
    const CommonStore& common_;const PersonalState& personal_;const PersonalCatalog& catalog_;const InteractionState& interaction_;
};
} // 适配器不分配结果、不修改战斗、不接受外部数量或伪造事件。
