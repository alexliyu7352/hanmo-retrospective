// 围城结果本身不代表本人已确认；新账只在原确认候选中写，不修改胜负/军令或资产。
#include "military_recovery_siege_release.hpp"
#include "military_roster_history.hpp"
#include "military_field_command_source.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <set>
namespace hanmo::resident {
namespace {
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& x:rows)if(x.id==id)return &x;return nullptr;}
bool has(const std::vector<StableId>& rows,const StableId& id){return std::find(rows.begin(),rows.end(),id)!=rows.end();}
bool involved(const SiegeRecord& s,const StableId& army){for(const auto& row:s.armies)if(row.first==army)return true;return false;}
Integer sequence(const StableId& id){const StableId prefix="military_release:";if(id.rfind(prefix,0)!=0)return -1;const auto tail=id.substr(prefix.size());Integer out=0;const auto p=std::from_chars(tail.data(),tail.data()+tail.size(),out);return p.ec==std::errc{}&&p.ptr==tail.data()+tail.size()&&out>0&&std::to_string(out)==tail?out:-1;}
std::optional<std::size_t> position(const CommonStore& c,const StableId& id){const auto& ids=c.state().facts.order();auto p=std::find(ids.begin(),ids.end(),id);return p==ids.end()?std::nullopt:std::optional<std::size_t>(std::size_t(p-ids.begin()));}
}
Status record_military_siege_release(CommonWrite& c,MilitaryState& m,const MilitaryCatalog& cat,const SiegeRecord& siege,const StableId& actor){
 if(!cat.recovery_policy)return common_success();const auto* person=c.actor(actor);if(!person||!person->alive||siege.phase!="result"||!siege.result||siege.result->tick<siege.started_tick||siege.result->tick>c.tick())return common_failure("MilitarySiegeReleaseSourceRequired",siege.id);for(const auto& f:m.recovery.siege_releases)if(f.siege_id==siege.id)return common_failure("MilitarySiegeReleaseRepeated",siege.id);
 MilitarySiegeReleaseFact f;f.siege_id=siege.id;f.actor_id=actor;f.tick=c.tick();f.result_tick=siege.result->tick;f.result_kind=siege.result->kind;f.winner_side=siege.result->winner_side;f.terms=siege.result->terms;f.reason=siege.result->reason;
 for(const auto& row:siege.armies){const auto* army=find(m.armies,row.first);if(army&&army->commander_actor_id==actor){f.army_id=army->id;f.authority_kind="commander";break;}}
 if(f.army_id.empty())for(const auto& army:m.armies)if(army.status=="active"&&has(army.member_actor_ids,actor)&&involved(siege,army.id)){
  bool allowed=!has(siege.withdrawn_army_ids,army.id);if(!allowed&&siege.result->kind=="withdrawn")for(const auto& event:siege.last_events)if(event.kind=="withdrawal_request"&&event.army_ids&&has(*event.army_ids,army.id))allowed=true;
  if(allowed){f.army_id=army.id;f.authority_kind="member";break;}
 }
 if(f.army_id.empty())return common_failure("MilitarySiegeReleaseAuthorityRequired",actor);auto id=c.next_id("military_release");if(!id.ok)return {false,{},id.error};f.id=id.value;auto written=c.append_fact({f.id,"military_siege_released","military"});if(!written.ok)return written;m.recovery.siege_releases.push_back(std::move(f));return common_success();
}
Status validate_military_siege_release_shape(const MilitaryRecoveryState& s){Integer last=0,tick=0;std::set<StableId> sieges;for(const auto& f:s.siege_releases){auto fields=f.field_order;std::sort(fields.begin(),fields.end());auto expected=std::vector<StableId>{"id","siege_id","actor_id","army_id","authority_kind","tick","result_tick","result_kind","winner_side","terms","reason"};std::sort(expected.begin(),expected.end());auto n=sequence(f.id);if(n<=last||f.tick<tick||f.result_tick<0||f.result_tick>f.tick||f.siege_id.empty()||f.actor_id.empty()||f.army_id.empty()||(f.authority_kind!="member"&&f.authority_kind!="commander")||f.result_kind.empty()||f.winner_side< -1||f.winner_side>1||(f.terms&&f.terms->empty())||(f.reason&&f.reason->empty())||!sieges.insert(f.siege_id).second||(!fields.empty()&&fields!=expected))return common_failure("MilitarySiegeReleaseShapeMismatch",f.id);last=n;tick=f.tick;}return common_success();}
Status validate_military_siege_release_history(const CommonStore& c,const MilitaryState& m,const MilitaryCatalog& cat,const DynamicStepInput* phase){
 Integer limit=c.tick();if(phase){if(phase->from_tick<0||phase->from_tick!=c.tick()||phase->from_tick==std::numeric_limits<Integer>::max()||phase->to_tick!=phase->from_tick+1)return common_failure("MilitaryRecoveryPhaseClockMismatch");limit=phase->to_tick;}if(!cat.recovery_policy)return m.recovery.siege_releases.empty()?common_success():common_failure("UnexpectedMilitarySiegeRelease");auto shaped=validate_military_siege_release_shape(m.recovery);if(!shaped.ok)return shaped;std::set<StableId> ids,released;
 for(const auto& f:m.recovery.siege_releases){const auto* h=c.state().facts.find(f.id);const auto* s=find(m.sieges,f.siege_id);const auto at=position(c,f.id);if(!h||h->kind!="military_siege_released"||h->domain!="military"||h->id!=f.id||!at||sequence(f.id)>=c.state().next_instance_id||f.tick>limit||!s||s->phase!="closed"||!s->result||s->result->tick!=f.result_tick||s->result->kind!=f.result_kind||s->result->winner_side!=f.winner_side||s->result->terms!=f.terms||s->result->reason!=f.reason||s->started_tick>f.result_tick||!involved(*s,f.army_id)||!c.actor(f.actor_id))return common_failure("MilitarySiegeReleaseSourceMismatch",f.id);
  for(const auto& casualty:m.casualty_facts)for(const auto& change:casualty.actor_changes)if(change.actor_id==f.actor_id&&change.status=="dead"){auto death=position(c,casualty.id);if(!death||*death<*at)return common_failure("MilitarySiegeReleaseActorDead",f.id);}
  if(f.authority_kind=="commander"){auto source=query_military_army_command_source_before(c,m,f.army_id,f.id);if(!source.ok)return {false,{},source.error};if(source.value.commander_actor_id!=f.actor_id)return common_failure("MilitarySiegeReleaseAuthorityRequired",f.id);}
  else{bool allowed=!has(s->withdrawn_army_ids,f.army_id);if(!allowed&&s->result->kind=="withdrawn")for(const auto& event:s->last_events)if(event.kind=="withdrawal_request"&&event.army_ids&&has(*event.army_ids,f.army_id))allowed=true;if(!allowed)return common_failure("MilitarySiegeReleaseAuthorityRequired",f.id);auto members=phase?military_army_actor_members_before(c,m,f.army_id,f.tick,f.id,*phase):military_army_actor_members_before(c,m,f.army_id,f.tick,f.id);if(!members.ok)return {false,{},members.error};if(!has(members.value,f.actor_id))return common_failure("MilitarySiegeReleaseAuthorityRequired",f.id);for(const auto& returned:m.return_facts)if(returned.army_id==f.army_id){auto order=position(c,returned.id);if(!order||*order<*at)return common_failure("MilitarySiegeReleaseAuthorityRequired",f.id);}}
  for(const auto& receipt:s->seen_receipt_ids){auto order=position(c,receipt);if(order&&*order>=*at)return common_failure("MilitarySiegeReleaseOrderMismatch",f.id);}
  ids.insert(f.id);released.insert(f.siege_id);
 }
 for(const auto& s:m.sieges)if(s.phase=="closed"&&!released.count(s.id))return common_failure("MilitarySiegeReleaseMissing",s.id);for(const auto& h:c.state().facts.rows())if(h.second.kind=="military_siege_released"&&h.second.domain=="military"&&!ids.count(h.first))return common_failure("MilitarySiegeReleaseMissing",h.first);return common_success();
}
}
