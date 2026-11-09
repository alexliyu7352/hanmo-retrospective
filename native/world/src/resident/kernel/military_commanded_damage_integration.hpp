// 只在原真实接敌栈借同候选人物资格；冷门等完整Family/Birth/Growth后复验。
#pragma once
#include "resident/domains/military/military_commanded_damage_contract.hpp"
#include "resident/model/dynamic_contract.hpp"
namespace hanmo::resident {
struct WorldBundle;
class MilitaryCommandedWorldRead final:public MilitaryCommandedSkillRead {
public:
 explicit MilitaryCommandedWorldRead(const WorldBundle& w):world_(w){}
 MilitaryCommandedWorldRead(const WorldBundle& w,const DynamicStepInput& p):world_(w),phase_(p){}
 Outcome<MilitaryCommandedSkillObservation> current_commanded_skill(const StableId&,const StableId&)const override;
 Outcome<MilitaryCommandedSkillObservation> commanded_skill_before_entry(const StableId&,const StableId&,const StableId&)const override;
private:
 const WorldBundle& world_;std::optional<DynamicStepInput> phase_;
};
Status validate_military_commanded_damage_world(const WorldBundle&);
}
