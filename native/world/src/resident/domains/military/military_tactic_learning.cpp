#include "resident/kernel/catalog_section_observation.hpp"
// 只校验封闭规则与机械形状；课程成功/初态/实践的权限在精确来源门另验。
#include "military_tactic_learning.hpp"
#include "resident/model/common_model.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
namespace {
const std::vector<StableId> names{"rally","guard","suppress","flank","reform","march","feint","support"};
bool exact_fields(FieldOrder actual,FieldOrder expected){if(actual.empty())return true;std::sort(actual.begin(),actual.end());std::sort(expected.begin(),expected.end());return actual==expected;}
bool unique(const std::vector<StableId>& values){std::set<StableId> seen;for(const auto& value:values)if(value.empty()||!seen.insert(value).second)return false;return true;}
}
StableId military_learned_tactic_kind_name(MilitaryLearnedTacticKind k){const auto i=static_cast<unsigned>(k);return i<names.size()?names[i]:StableId{};}
Outcome<MilitaryLearnedTacticKind> military_learned_tactic_kind(const StableId& name){auto it=std::find(names.begin(),names.end(),name);if(it==names.end())return {false,{},common_failure("UnknownMilitaryLearnedTactic",name).error};return {true,static_cast<MilitaryLearnedTacticKind>(it-names.begin()),{}};}
Outcome<MilitaryLearnedTacticRule> military_learned_tactic_rule(MilitaryLearnedTacticKind k){
 using K=MilitaryLearnedTacticKind;switch(k){
 case K::rally:return {true,{k,"tactics",1,15},{}};case K::guard:return {true,{k,"infantry",1,15},{}};case K::suppress:return {true,{k,"archery",1,20},{}};case K::flank:return {true,{k,"cavalry",2,20},{}};case K::reform:return {true,{k,"tactics",1,15},{}};case K::march:return {true,{k,"transport",2,15},{}};case K::feint:return {true,{k,"tactics",2,20},{}};case K::support:return {true,{k,"infantry",2,20},{}};}
 return {false,{},common_failure("UnknownMilitaryLearnedTactic").error};
}
Status validate_military_tactic_learning_catalog(const MilitaryTacticLearningCatalog& c){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 84, "MilitaryTacticLearningCatalog", "validate_military_tactic_learning_catalog", &c);

 if(!c.enabled)return c.definitions.empty()&&c.initial_grants.empty()?common_success():common_failure("MilitaryTacticLearningProfileRequired");if(c.definitions.empty()||c.definitions.size()>256||c.initial_grants.size()>16384)return common_failure("MilitaryTacticLearningCatalogLimit");std::set<StableId> definitions,grants;std::set<std::pair<StableId,StableId>> persons;
 for(const auto& d:c.definitions){if(d.id.empty()||!definitions.insert(d.id).second||d.name_key.empty()||d.description_key.empty()||!military_learned_tactic_rule(d.kind).ok||d.teacher_actor_ids.empty()||d.practice_challenge_ids.empty()||!unique(d.teacher_actor_ids)||!unique(d.practice_challenge_ids)||d.practice_min_score<0||d.practice_min_score>100)return common_failure("MilitaryTacticLearningDefinitionInvalid",d.id);}
 for(const auto& g:c.initial_grants)if(g.id.empty()||!grants.insert(g.id).second||g.actor_id.empty()||!definitions.count(g.tactic_id)||!persons.emplace(g.actor_id,g.tactic_id).second)return common_failure("MilitaryTacticInitialGrantInvalid",g.id);return common_success();
}
Status validate_military_tactic_learning_shape(const MilitaryTacticLearningState& s){
 if(!exact_fields(s.field_order,{"acquisitions","practices"})||s.acquisitions.size()>65536||s.practices.size()>65536)return common_failure("MilitaryTacticLearningShapeMismatch");std::set<StableId> ids;std::set<std::pair<StableId,StableId>> learned;
 for(const auto& f:s.acquisitions){const auto kind=static_cast<unsigned>(f.source_kind);if(f.id.empty()||f.actor_id.empty()||f.tactic_id.empty()||f.source_id.empty()||kind>2||f.tick<0||!ids.insert(f.id).second||!learned.emplace(f.actor_id,f.tactic_id).second||!exact_fields(f.field_order,{"id","actor_id","tactic_id","source_id","source_kind","tick"}))return common_failure("MilitaryTacticAcquisitionShapeMismatch",f.id);if(f.source_kind==MilitaryTacticLearningSourceKind::authored_initial&&f.tick!=0)return common_failure("MilitaryTacticInitialBoundaryMismatch",f.id);}
 std::set<StableId> practice_ids,trainings;for(const auto& f:s.practices)if(f.id.empty()||f.actor_id.empty()||f.tactic_id.empty()||f.training_action_id.empty()||f.tick<0||!practice_ids.insert(f.id).second||!trainings.insert(f.training_action_id).second||!exact_fields(f.field_order,{"id","actor_id","tactic_id","training_action_id","tick"}))return common_failure("MilitaryTacticPracticeShapeMismatch",f.id);
 return common_success();
}
}
