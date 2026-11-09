// 只在同一真实contact栈借S4兵法源；冷人物资格必须在完整Family/Birth/Growth之后。
#pragma once
#include "resident/domains/military/military_command_resources_contract.hpp"
#include "resident/model/dynamic_contract.hpp"
namespace hanmo::resident {
struct WorldBundle;
class MilitaryTacticsWorldRead final:public MilitaryTacticsEntryRead {
public:
 explicit MilitaryTacticsWorldRead(const WorldBundle& w):world_(w){}
 MilitaryTacticsWorldRead(const WorldBundle& w,const DynamicStepInput& phase):world_(w),phase_(phase){}
 Outcome<MilitaryTacticsEntryObservation> current_tactics_entry(const StableId&)const override;
 Outcome<MilitaryTacticsEntryObservation> tactics_before_entry(const StableId&,const StableId&)const override;
private:
 const WorldBundle& world_;std::optional<DynamicStepInput> phase_;
};
Status validate_military_command_resources_world(const WorldBundle&);
}
