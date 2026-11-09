// 实际Growth技艺与原Naval入战准备之间的薄桥；只借一个候选，无第二XP或军事世界。
#pragma once
#include "resident/domains/military/naval_gunnery_skill_contract.hpp"
#include "resident/model/dynamic_contract.hpp"
namespace hanmo::resident {
struct WorldBundle;
// phase按值保存，只由原contact_install栈构造；冷读取不带phase、不收任意未来tick。
class NavalGunneryWorldRead final:public NavalGunnerySkillRead {
public:
 explicit NavalGunneryWorldRead(const WorldBundle& world):world_(world){}
 NavalGunneryWorldRead(const WorldBundle& world,const DynamicStepInput& phase):world_(world),phase_(phase){}
 Outcome<NavalGunnerySkillObservation> current_naval_skill(const StableId&)const override;
 Outcome<NavalGunnerySkillObservation> naval_skill_before(const StableId&,const StableId&)const override;
private:
 const WorldBundle& world_;std::optional<DynamicStepInput> phase_;
};
// 只在同候选Family/Birth/Growth原最终校验后调用；早Naval6结构门不是此完整证明。
Status validate_naval_gunnery_world(const WorldBundle&);
}
