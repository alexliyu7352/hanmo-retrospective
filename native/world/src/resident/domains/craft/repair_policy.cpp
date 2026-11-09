#include "resident/kernel/catalog_section_observation.hpp"
#include "repair_policy.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
Status invalid(const char*code,const StableId&id={}){return common_failure(code,id);}
}
Status validate_repair_policy_catalog(const RepairPolicyCatalog&catalog,const CommonCatalog&common){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 20, "RepairPolicyCatalog", "validate_repair_policy_catalog", &catalog);

 for(const auto&row:catalog.policies.rows()){
  const auto&p=row.second;if(row.first.empty()||row.first!=p.policy_id||p.version<1||p.applicable_definitions.empty()||p.required_skill_id.empty()||!common.personal_skill_kinds.find(p.required_skill_id)||p.minimum_skill_level<0||p.minimum_equipment_level<0||p.workstation_slot_kind.empty()||p.duration_half_days<1||p.fee_qian<0||p.fee_policy_ref.empty()||p.custody_policy_ref.empty())return invalid("InvalidRepairPolicy",row.first);
  if(p.condition_rule!=RepairConditionRule::restore_existing_maximum)return invalid("UnsupportedRepairConditionRule",row.first);
  for(std::size_t i=0;i<p.applicable_definitions.size();++i)if(!common.items.find(p.applicable_definitions[i])||(i&&p.applicable_definitions[i-1]>=p.applicable_definitions[i]))return invalid("RepairDefinitionSetRequired",row.first);
  for(std::size_t i=0;i<p.materials.size();++i){const auto&m=p.materials[i];if(m.quantity<1||!common.items.find(m.definition_id))return invalid("RepairMaterialDefinitionRequired",row.first);for(std::size_t j=0;j<i;++j)if(p.materials[j].definition_id==m.definition_id)return invalid("DuplicateRepairMaterialDefinition",row.first);}
 }
 return common_success();
}
Outcome<const RepairPolicy*> read_repair_policy(const RepairPolicyCatalog&c,const RepairPolicyRef&r){const auto*p=c.policies.find(r.policy_id);if(!p||r.version<1||p->policy_id!=r.policy_id||p->version!=r.version)return {false,nullptr,invalid("RepairPolicyVersionRequired",r.policy_id).error};return {true,p,{}};}
Outcome<RepairConditionTarget> read_repair_condition_target(const RepairPolicy&p,const ItemInstance&i,Integer maximum){
 if(p.policy_id!=i.repair_policy_id||maximum<1||std::find(p.applicable_definitions.begin(),p.applicable_definitions.end(),i.item_definition_id)==p.applicable_definitions.end())return {false,{},invalid("RepairPolicyItemMismatch",i.id).error};
 if(p.condition_rule!=RepairConditionRule::restore_existing_maximum)return {false,{},invalid("UnsupportedRepairConditionRule",p.policy_id).error};
 return {true,{maximum,maximum},{}};
}
}
