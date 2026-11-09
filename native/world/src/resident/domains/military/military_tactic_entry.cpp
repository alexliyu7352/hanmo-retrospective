// 结构完整覆盖真实参战单位；作者仅列可用模板，本人是否学会由原事实引用决定。
#include "military_tactic_entry.hpp"
#include "resident/model/common_model.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
namespace {
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& r:rows)if(r.id==id)return &r;return nullptr;}
bool fields(FieldOrder a,FieldOrder b){if(a.empty())return true;std::sort(a.begin(),a.end());std::sort(b.begin(),b.end());return a==b;}
}
Status validate_military_tactic_entry_shape(const MilitaryTacticEntryState& s){
 if(s.entry_tick<0||s.units.empty()||s.units.size()>64||!fields(s.field_order,{"entry_tick","units"}))return common_failure("MilitaryTacticEntryShapeMismatch");StableId previous;
 for(const auto& u:s.units){if(u.unit_id.empty()||u.commander_actor_id.empty()||(!previous.empty()&&u.unit_id<=previous)||u.tactic_acquisition_ids.size()>256||!fields(u.field_order,{"unit_id","commander_actor_id","tactic_acquisition_ids"}))return common_failure("MilitaryTacticEntryUnitMismatch",u.unit_id);previous=u.unit_id;StableId last;std::set<StableId> sources;for(const auto& p:u.tactic_acquisition_ids){if(p.first.empty()||p.second.empty()||(!last.empty()&&p.first<=last)||!sources.insert(p.second).second)return common_failure("MilitaryTacticEntryAcquisitionMismatch",u.unit_id);last=p.first;}}
 return common_success();
}
Status validate_military_learned_tactic_binding(const MilitaryTactic& t,const MilitaryTacticLearningCatalog& cat){
 const auto* d=find(cat.definitions,t.learned_tactic_id);if(!cat.enabled||!d||t.kind!=military_learned_tactic_kind_name(d->kind))return common_failure("MilitaryTacticDefinitionBindingMismatch",t.id);auto rule=military_learned_tactic_rule(d->kind);if(!rule.ok)return {false,{},rule.error};if(t.point_cost!=rule.value.point_cost)return common_failure("MilitaryTacticDefinitionCostMismatch",t.id);
 const Integer power=t.kind=="suppress"?8000:t.kind=="flank"?12500:10000,move=t.kind=="march"?2:0,morale=t.kind=="rally"?10:0;
 if(t.power_bp!=power||t.move_bonus!=move||t.morale_effect!=morale||t.accuracy_bonus!=0||t.status_rounds!=1)return common_failure("MilitaryTacticDefinitionEffectMismatch",t.id);return common_success();
}
Status validate_military_tactic_entry_bindings(const MilitaryBattleRecord& b,const MilitaryBattleCatalog& c,const MilitaryTacticEntryState& s){
 auto checked=validate_military_tactic_entry_shape(s);if(!checked.ok)return checked;if(!c.tactic_learning||!b.command_resources||!b.rout_retreat_enabled||!c.sequential_round_rule||s.entry_tick!=b.created_tick||s.units.size()!=b.units.size())return common_failure("MilitaryTacticEntryPrerequisiteRequired",b.id);checked=validate_military_tactic_learning_catalog(*c.tactic_learning);if(!checked.ok)return checked;
 std::set<StableId> units;for(const auto& row:s.units){const BattleUnit* u=nullptr;for(const auto& actual:b.units)if(actual.unit_id==row.unit_id)u=&actual;if(!u||!units.insert(row.unit_id).second||u->commander_actor_id!=row.commander_actor_id)return common_failure("MilitaryTacticEntryUnitMismatch",row.unit_id);const auto* troop=find(c.troop_types,u->troop_type_id);if(!troop)return common_failure("UnknownMilitaryDefinition",u->troop_type_id);std::set<StableId> definitions;for(const auto& t:troop->tactics){checked=validate_military_learned_tactic_binding(t,*c.tactic_learning);if(!checked.ok)return checked;if(!definitions.insert(t.learned_tactic_id).second)return common_failure("MilitaryTacticDefinitionRepeated",t.id);}for(const auto& p:row.tactic_acquisition_ids)if(!definitions.count(p.first))return common_failure("MilitaryTacticEntryUnexpectedDefinition",p.first);}
 return common_success();
}
Status require_military_learned_tactic(const MilitaryBattleRecord& b,const MilitaryBattleCatalog& c,const StableId& unit,const MilitaryTactic& t){
 if(!b.learned_tactics)return t.learned_tactic_id.empty()?common_success():common_failure("MilitaryTacticLearningProfileRequired",t.id);if(!c.tactic_learning)return common_failure("MilitaryTacticLearningProfileRequired",t.id);auto checked=validate_military_learned_tactic_binding(t,*c.tactic_learning);if(!checked.ok)return checked;for(const auto& row:b.learned_tactics->units)if(row.unit_id==unit){for(const auto& p:row.tactic_acquisition_ids)if(p.first==t.learned_tactic_id)return common_success();return common_failure("MilitaryTacticNotLearned",t.learned_tactic_id);}return common_failure("MilitaryTacticEntryUnitMismatch",unit);
}
}
