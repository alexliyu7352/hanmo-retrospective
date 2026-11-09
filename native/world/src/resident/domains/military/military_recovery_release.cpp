#include "resident/domains/military/military_model.hpp"
// 记录的是原确认命令的真实提交，不是current closed、时间流逝或零时补造的过去退出。
#include "military_recovery_release.hpp"
#include "military_commander.hpp"
#include "resident/model/dynamic_contract.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <set>
namespace hanmo::resident {
namespace {
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& row:rows)if(row.id==id)return &row;return nullptr;}
bool has(const std::vector<StableId>& ids,const StableId& id){return std::find(ids.begin(),ids.end(),id)!=ids.end();}
Integer sequence(const StableId& id){const StableId prefix="military_release:";if(id.rfind(prefix,0)!=0)return -1;const auto tail=id.substr(prefix.size());Integer out=0;const auto parsed=std::from_chars(tail.data(),tail.data()+tail.size(),out);return parsed.ec==std::errc{}&&parsed.ptr==tail.data()+tail.size()&&out>0&&std::to_string(out)==tail?out:-1;}
std::optional<std::size_t> order(const CommonStore& c,const StableId& id){const auto& ids=c.state().facts.order();auto at=std::find(ids.begin(),ids.end(),id);return at==ids.end()?std::nullopt:std::optional<std::size_t>(std::size_t(at-ids.begin()));}
// 自主/原失权终止/本轮控制者死亡均已有实际最终伤亡源，不追加假手动确认。
bool automatic(const MilitaryState& m,const MilitaryBattleRecord& b){if(b.controller_actor_id.empty()||(b.result&&has({"authority_lost","responsible_actor_lost","truce","cancelled","timeout"},b.result->reason)))return true;const auto* f=find(m.casualty_facts,b.settlement_receipt_id);if(f)for(const auto& change:f->actor_changes)if(change.actor_id==b.controller_actor_id&&change.status=="dead")return true;return false;}
}
Status record_military_battle_release(CommonWrite& c,MilitaryState& m,const MilitaryCatalog& cat,const MilitaryAccess& access,const MilitaryBattleRecord& b,const StableId& actor){
 if(!cat.recovery_policy)return common_success();const auto* person=c.actor(actor);const auto* final=find(m.casualty_facts,b.settlement_receipt_id);if(!person||!person->alive||actor!=access.player_actor_id||actor!=b.controller_actor_id||b.phase!="result"||!b.applied||!final||final->battle_id!=b.id||final->tick!=b.completed_tick||final->tick>c.tick()||automatic(m,b)||!access.fact_exists||!access.fact_exists(final->id))return common_failure("MilitaryBattleReleaseSourceRequired",b.id);
 for(const auto& f:m.recovery.battle_releases)if(f.battle_id==b.id)return common_failure("MilitaryBattleReleaseRepeated",b.id);auto id=c.next_id("military_release");if(!id.ok)return {false,{},id.error};MilitaryBattleReleaseFact f;f.id=id.value;f.battle_id=b.id;f.actor_id=actor;f.settlement_receipt_id=final->id;f.tick=c.tick();auto written=c.append_fact({f.id,"military_battle_released","military"});if(!written.ok)return written;m.recovery.battle_releases.push_back(std::move(f));return common_success();
}
Status validate_military_battle_release_shape(const MilitaryRecoveryState& s){Integer last=0,tick=0;std::set<StableId> battles;for(const auto& f:s.battle_releases){const auto n=sequence(f.id);auto fields=f.field_order;std::sort(fields.begin(),fields.end());auto expected=std::vector<StableId>{"id","battle_id","actor_id","settlement_receipt_id","tick"};std::sort(expected.begin(),expected.end());if(n<=last||f.tick<tick||f.battle_id.empty()||f.actor_id.empty()||f.settlement_receipt_id.empty()||!battles.insert(f.battle_id).second||(!fields.empty()&&fields!=expected))return common_failure("MilitaryBattleReleaseShapeMismatch",f.id);last=n;tick=f.tick;}return common_success();}
Status validate_military_battle_release_history(const CommonStore& c,const MilitaryState& m,const MilitaryCatalog& cat,const DynamicStepInput* phase){
 Integer limit=c.tick();if(phase){if(phase->from_tick<0||phase->from_tick!=c.tick()||phase->from_tick==std::numeric_limits<Integer>::max()||phase->to_tick!=phase->from_tick+1)return common_failure("MilitaryRecoveryPhaseClockMismatch");limit=phase->to_tick;}if(!cat.recovery_policy)return m.recovery.battle_releases.empty()?common_success():common_failure("UnexpectedMilitaryBattleRelease");auto shaped=validate_military_battle_release_shape(m.recovery);if(!shaped.ok)return shaped;std::set<StableId> ids,released;
 for(const auto& f:m.recovery.battle_releases){const auto* head=c.state().facts.find(f.id);const auto* b=find(m.battles,f.battle_id);const auto* result=find(m.casualty_facts,f.settlement_receipt_id);const auto* result_head=c.state().facts.find(f.settlement_receipt_id);const auto from=order(c,f.settlement_receipt_id),to=order(c,f.id);if(sequence(f.id)>=c.state().next_instance_id||!head||head->id!=f.id||head->kind!="military_battle_released"||head->domain!="military"||f.tick>limit||!b||b->phase!="closed"||!b->applied||b->controller_actor_id!=f.actor_id||b->settlement_receipt_id!=f.settlement_receipt_id||!result||result->battle_id!=b->id||result->tick!=b->completed_tick||result->tick>f.tick||!result_head||result_head->kind!="military_casualties"||result_head->domain!="military"||!from||!to||*from>=*to||automatic(m,*b)||!c.actor(f.actor_id))return common_failure("MilitaryBattleReleaseSourceMismatch",f.id);
  for(const auto& returned:m.return_facts)if(has(b->army_ids,returned.army_id)&&returned.tick>=result->tick){const auto end=order(c,returned.id);if(!end||*end<=*to)return common_failure("MilitaryBattleReleaseReturnOrderMismatch",f.id);for(const auto& action:m.logistics_facts)if(action.result&&action.result->receipt_id==returned.id){const auto started=order(c,action.id);if(!started||*started<=*to)return common_failure("MilitaryBattleReleaseReturnOrderMismatch",f.id);}}
  for(const auto& loss:m.casualty_facts)for(const auto& change:loss.actor_changes)if(change.actor_id==f.actor_id&&change.status=="dead"){const auto death=order(c,loss.id);if(!death||*death<*to)return common_failure("MilitaryBattleReleaseActorDead",f.id);}ids.insert(f.id);released.insert(b->id);
 }
 for(const auto& b:m.battles)if(b.phase=="closed"&&!automatic(m,b)&&!released.count(b.id))return common_failure("MilitaryBattleReleaseMissing",b.id);
 for(const auto& h:c.state().facts.rows())if(h.second.kind=="military_battle_released"&&h.second.domain=="military"&&!ids.count(h.first))return common_failure("MilitaryBattleReleaseMissing",h.first);return common_success();
}
}
