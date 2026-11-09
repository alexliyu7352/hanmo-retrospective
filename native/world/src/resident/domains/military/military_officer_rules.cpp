#include "resident/domains/military/military_model.hpp"
// 调任写入只改一个现部队的军官引用和一个永久事实；原兵籍、公资、军团与舟行不变。
#include "military_officer_rules.hpp"
#include "occupation_rules.hpp"
#include "resident/model/common_operations.hpp"
#include <algorithm>
#include <charconv>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
template<class T>T* find(std::vector<T>& rows,const StableId& id){for(auto& row:rows)if(row.id==id)return &row;return nullptr;}
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& row:rows)if(row.id==id)return &row;return nullptr;}
template<class T>Outcome<T> fail(const StableId& code,const StableId& id={}){return {false,{},common_failure(code,id).error};}
bool same(const MilitaryUnitCommandSource& a,const MilitaryUnitCommandSource& b){return a.kind==b.kind&&a.id==b.id;}
bool same(const MilitaryOfficerAssignmentTarget& a,const MilitaryOfficerAssignmentTarget& b){return std::tie(a.organization_id,a.garrison_id,a.unit_id,a.previous_actor_id,a.next_actor_id)==std::tie(b.organization_id,b.garrison_id,b.unit_id,b.previous_actor_id,b.next_actor_id)&&same(a.previous_source,b.previous_source);}
bool same(const WorldIdentity& a,const WorldIdentity& b){return std::tie(a.world_id,a.execution_epoch,a.revision,a.tick)==std::tie(b.world_id,b.execution_epoch,b.revision,b.tick);}
bool key(const StableId& s){return !s.empty()&&s.size()<=128&&std::all_of(s.begin(),s.end(),[](unsigned char c){return c>=33&&c<=126;});}
bool at(const CommonRead& c,const StableId& actor,const StableId& place){const auto* p=c.actor(actor);const auto* here=p?std::get_if<SettlementLocation>(&p->location):nullptr;return here&&here->place_id==place;}
bool ready(const CommonRead& c,const MilitaryState& s,const MilitaryAccess& a,const StableId& id){const auto* p=c.actor(id);return p&&p->alive&&p->available&&p->hp>0&&p->action_id.empty()&&!active_army_for_actor(s,id)&&(!a.in_open_battle||!a.in_open_battle(id))&&(!a.in_party||!a.in_party(id));}
Integer sequence(const StableId& id){auto at=id.rfind(':');if(at==StableId::npos)return -1;Integer n=0;auto suffix=id.substr(at+1);auto r=std::from_chars(suffix.data(),suffix.data()+suffix.size(),n);return r.ec==std::errc{}&&r.ptr==suffix.data()+suffix.size()&&n>0&&std::to_string(n)==suffix?n:-1;}
bool proof_shape(const MilitaryOfficerAppointmentProof& p){return !p.target.organization_id.empty()&&!p.target.garrison_id.empty()&&!p.target.unit_id.empty()&&!p.target.previous_actor_id.empty()&&!p.target.next_actor_id.empty()&&p.target.previous_actor_id!=p.target.next_actor_id&&!p.target.previous_source.id.empty()&&!p.authorization_fact_id.empty()&&!p.acceptance_fact_id.empty()&&p.authorization_fact_id!=p.acceptance_fact_id&&!p.issuer_actor_id.empty()&&!p.office_id.empty()&&!p.grant_id.empty()&&p.authority_epoch>=0&&p.issued_tick>=0&&p.accepted_tick>=p.issued_tick&&p.observed_tick>=p.accepted_tick;}
MilitaryOfficerAssignmentFact freeze(const MilitaryOfficerAppointmentProof& p,const StableId& actor,const MilitaryOfficerAssign& r,const StableId& place){MilitaryOfficerAssignmentFact f;f.request_key=r.request_key;f.actor_id=actor;f.authorization_fact_id=p.authorization_fact_id;f.acceptance_fact_id=p.acceptance_fact_id;f.place_id=place;f.target=p.target;f.issuer_actor_id=p.issuer_actor_id;f.office_id=p.office_id;f.grant_id=p.grant_id;f.authority_epoch=p.authority_epoch;f.issued_tick=p.issued_tick;f.accepted_tick=p.accepted_tick;f.tick=p.observed_tick;return f;}
bool matches(const MilitaryOfficerAssignmentFact& f,const MilitaryOfficerAppointmentProof& p){return proof_shape(p)&&same(f.target,p.target)&&std::tie(f.authorization_fact_id,f.acceptance_fact_id,f.issuer_actor_id,f.office_id,f.grant_id,f.authority_epoch,f.issued_tick,f.accepted_tick,f.tick)==std::tie(p.authorization_fact_id,p.acceptance_fact_id,p.issuer_actor_id,p.office_id,p.grant_id,p.authority_epoch,p.issued_tick,p.accepted_tick,p.observed_tick);}
}
Outcome<MilitaryOfficerAssignmentQuote> preview_military_officer_assignment(const CommonRead& c,const MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& a,const StableId& actor,const MilitaryOfficerAssign& r){
 using Q=MilitaryOfficerAssignmentQuote;if(!cat.military||!cat.officer_assignment_policy||cat.officer_assignment_policy->profile!="political_appointment_v1")return fail<Q>("MilitaryOfficerAssignmentProfileRequired");
 if(!key(r.request_key)||r.authorization_fact_id.empty()||r.acceptance_fact_id.empty())return fail<Q>("InvalidMilitaryOfficerAssignment");
 for(const auto& f:s.officer_assignments)if(f.actor_id==actor&&f.request_key==r.request_key){if(f.authorization_fact_id!=r.authorization_fact_id||f.acceptance_fact_id!=r.acceptance_fact_id)return fail<Q>("MilitaryOfficerAssignmentKeyConflict",r.request_key);return {true,{true,f},{}};}
 if(!a.officer_appointments||!a.unit_command_source)return fail<Q>("MilitaryOfficerAppointmentReadRequired");
 auto p=a.officer_appointments->appointment_for_application(r.authorization_fact_id,r.acceptance_fact_id,actor);if(!p.ok)return {false,{},p.error};
 if(!proof_shape(p.value)||p.value.authorization_fact_id!=r.authorization_fact_id||p.value.acceptance_fact_id!=r.acceptance_fact_id||p.value.target.next_actor_id!=actor||p.value.observed_tick!=c.tick()||!a.fact_exists||!a.fact_exists(r.authorization_fact_id)||!a.fact_exists(r.acceptance_fact_id))return fail<Q>("MilitaryOfficerAppointmentMismatch");
 for(const auto& f:s.officer_assignments)if(f.authorization_fact_id==r.authorization_fact_id||f.acceptance_fact_id==r.acceptance_fact_id)return fail<Q>("MilitaryOfficerAppointmentConsumed",r.authorization_fact_id);
 const auto& target=p.value.target;const auto* unit=find(s.military_units,target.unit_id);const auto* camp=find(cat.garrisons,target.garrison_id);if(!unit||!camp)return fail<Q>("MilitaryOfficerUnitMissing",target.unit_id);
 if(!unit->army_id.empty()||unit->garrison_id!=camp->id||!unit->mobilizing_action_id.empty()||(unit->training_action_id&&!unit->training_action_id->empty()))return fail<Q>("MilitaryOfficerUnitOccupied",unit->id);
 auto owner=military_garrison_owner(c,s,cat,a,camp->id);if(!owner.ok)return {false,{},owner.error};if(owner.value.organization_id!=target.organization_id||unit->organization_id!=target.organization_id)return fail<Q>("MilitaryOfficerOrganizationChanged",unit->id);
 auto current=a.unit_command_source(unit->id);if(!current.ok)return {false,{},current.error};if(!same(current.value.source_identity,p.value.source_identity)||current.value.source_identity.tick!=c.tick()||current.value.unit_id!=unit->id||current.value.organization_id!=target.organization_id||current.value.garrison_id!=camp->id||!current.value.army_id.empty()||current.value.commander_actor_id!=target.previous_actor_id||unit->commander_actor_id!=target.previous_actor_id||!same(current.value.command_source,target.previous_source))return fail<Q>("MilitaryOfficerCommandSourceChanged",unit->id);
 if(!ready(c,s,a,actor)||!at(c,actor,camp->place_id))return fail<Q>("MilitaryOfficerCandidateUnavailable",actor);
 for(const auto& other:s.military_units)if(other.id!=unit->id&&other.commander_actor_id==actor)return fail<Q>("MilitaryOfficerAlreadyCommands",actor);
 const auto* previous=c.actor(target.previous_actor_id);if(!previous)return fail<Q>("UnknownActor",target.previous_actor_id);if(previous->alive&&(!ready(c,s,a,previous->id)||!at(c,previous->id,camp->place_id)))return fail<Q>("MilitaryOfficerPredecessorOccupied",previous->id);
 for(const auto& battle:s.battles)if(battle.phase!="closed")for(const auto& u:battle.units)if(u.unit_id==unit->id)return fail<Q>("MilitaryOfficerUnitEngaged",unit->id);
 for(const auto& siege:s.sieges)if(siege.phase!="closed")for(const auto& occupied:siege.unit_nodes)if(occupied.first==unit->id)return fail<Q>("MilitaryOfficerUnitEngaged",unit->id);
 return {true,{false,freeze(p.value,actor,r,camp->place_id)},{}};
}
Outcome<MilitaryEffect> apply_military_officer_assignment(CommonWrite& c,MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& a,const StableId& actor,const MilitaryOfficerAssign& request){auto q=preview_military_officer_assignment(c,s,cat,a,actor,request);if(!q.ok)return {false,{},q.error};auto fact=std::move(q.value.fact);if(!q.value.replayed){auto id=c.next_id("military_assignment");if(!id.ok)return {false,{},id.error};fact.id=id.value;auto recorded=c.append_fact({fact.id,"military_officer_assignment","military"});if(!recorded.ok)return {false,{},recorded.error};find(s.military_units,fact.target.unit_id)->commander_actor_id=actor;s.officer_assignments.push_back(fact);}MilitaryEffect out;out.receipt_id=fact.id;out.source_id=fact.authorization_fact_id;out.office_id=fact.office_id;out.already_completed=q.value.replayed;return {true,std::move(out),{}};}
// 冷历史以原fact前名册及专项授权核每次交接；当前权力变更不抹除旧合法调任。
Status validate_military_officer_assignments(const CommonStore& c,const MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& a){
 if(!cat.officer_assignment_policy)return s.officer_assignments.empty()?common_success():common_failure("UnexpectedMilitaryOfficerAssignments");
 if(cat.officer_assignment_policy->profile!="political_appointment_v1"||!a.officer_appointments||!a.rosters_before)return common_failure("MilitaryOfficerAppointmentReadRequired");
 std::set<StableId> ids,authorizations,acceptances;std::set<std::pair<StableId,StableId>> keys;Integer last_sequence=0,last_tick=-1;
 for(const auto& f:s.officer_assignments){const auto seq=sequence(f.id);const auto* head=c.state().facts.find(f.id);const auto* camp=find(cat.garrisons,f.target.garrison_id);if(f.id.rfind("military_assignment:",0)!=0||seq<=last_sequence||seq>=c.state().next_instance_id||f.tick<last_tick||f.tick>c.tick()||!head||head->kind!="military_officer_assignment"||head->domain!="military"||!key(f.request_key)||!ids.insert(f.id).second||!keys.insert({f.actor_id,f.request_key}).second||!authorizations.insert(f.authorization_fact_id).second||!acceptances.insert(f.acceptance_fact_id).second||f.actor_id!=f.target.next_actor_id||!camp||camp->place_id!=f.place_id)return common_failure("MilitaryOfficerAssignmentHistoryMismatch",f.id);
  for(const auto& id:{f.authorization_fact_id,f.acceptance_fact_id})if(!c.state().facts.find(id)||sequence(id)<1||sequence(id)>=seq)return common_failure("MilitaryOfficerAppointmentOrderMismatch",f.id);
  auto proof=a.officer_appointments->appointment_before_application(f.authorization_fact_id,f.acceptance_fact_id,f.id,f.tick);if(!proof.ok)return {false,{},proof.error};if(!same(proof.value.source_identity,c.state().identity)||!matches(f,proof.value))return common_failure("MilitaryOfficerAppointmentMismatch",f.id);
  auto prior=query_military_unit_command_source_before(c,s,cat,f.target.unit_id,f.id);if(!prior.ok)return {false,{},prior.error};if(prior.value.commander_actor_id!=f.target.previous_actor_id||!same(prior.value.command_source,f.target.previous_source))return common_failure("MilitaryOfficerCommandSourceChanged",f.id);
  auto rosters=a.rosters_before({f.target.unit_id},f.tick,f.id);if(!rosters.ok)return {false,{},rosters.error};if(rosters.value.size()!=1)return common_failure("MilitaryOfficerUnitMissing",f.id);const auto& u=rosters.value.front();if(u.id!=f.target.unit_id||u.commander_actor_id!=f.target.previous_actor_id||u.organization_id!=f.target.organization_id||u.garrison_id!=f.target.garrison_id||!u.army_id.empty())return common_failure("MilitaryOfficerUnitOccupied",f.id);
  for(const auto& other:s.military_units)if(other.id!=f.target.unit_id){auto command=query_military_unit_command_source_before(c,s,cat,other.id,f.id);if(!command.ok)return {false,{},command.error};if(command.value.commander_actor_id==f.target.next_actor_id)return common_failure("MilitaryOfficerAlreadyCommands",f.id);}
  for(const auto& training:s.training_facts)if(training.terms.unit_id==f.target.unit_id&&sequence(training.id)<seq&&(training.phase!="completed"||training.completed_tick>f.tick))return common_failure("MilitaryOfficerUnitOccupied",f.id);
  last_sequence=seq;last_tick=f.tick;
 }
 for(const auto& unit:s.military_units){auto actual=query_military_unit_command_source(c,s,cat,unit.id);if(!actual.ok)return {false,{},actual.error};}
 return common_success();
}
// 缓存沿原MilitaryEffect外壳，只允许本业务实际写出的三个来源字段和幂等标记。
bool military_officer_effect_shape(const MilitaryEffect& e){return !e.receipt_id.empty()&&!e.source_id.empty()&&!e.office_id.empty()&&!(!e.already_completed.has_value()||e.request_answer||!e.action_id.empty()||e.training_receipt||!e.completed_receipt_ids.empty()||!e.fact_id.empty()||e.return_wait_reason||!e.battle_id.empty()||!e.siege_id.empty()||e.plan_submitted||!e.actor_changes.empty()||!e.journey_requests.empty()||!e.success||!e.code.empty()||!e.army_id.empty()||!e.order_id.empty()||!e.campaign_id.empty()||e.details_ok||e.journey_request||e.integration_request);}
Status validate_military_officer_cached_effect(const MilitaryState& s,const MilitaryEffect& e,const StableId& actor){const auto* f=find(s.officer_assignments,e.receipt_id);if(!f||f->actor_id!=actor||e.source_id!=f->authorization_fact_id||e.office_id!=f->office_id||!military_officer_effect_shape(e))return common_failure("MilitaryOfficerCachedEffectMismatch",e.receipt_id);return common_success();}

}
