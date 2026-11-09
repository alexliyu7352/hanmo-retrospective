// 玩家控制变化须同时位于本次生前交接事实的分配序/Common序同侧，今日metadata不倒填旧签字。
#include "political_successor_round_control.hpp"
#include "resident/domains/family/historical_player.hpp"
#include "resident/domains/family/family_legacy.hpp"
#include "resident/domains/family/voluntary_succession.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include <set>
namespace hanmo::resident {
Outcome<StableId> read_political_successor_round_player_before_fact(const CommonStore& common,const FamilyState& family,const FamilyLegacyState* legacy,Integer tick,const StableId& boundary){
 auto fail=[&](const StableId& code,const StableId& id)->Outcome<StableId>{return {false,{},common_failure(code,id).error};};auto sequence=political_successor_round_fact_sequence(common,boundary);const auto& order=common.state().facts.order();const auto end=std::find(order.begin(),order.end(),boundary);if(!sequence||end==order.end()||tick<0||tick>common.tick())return fail("PoliticalSuccessorRoundPlayerBoundaryMismatch",boundary);std::set<Integer> used{*sequence};
 // 原三类控制来源分别严格检查原前缀/头与唯一编号，再由Family给最终合法玩家。
 auto crossing=[&](const StableId& id,const StableId& prefix,const StableId& kind,const StableId& domain,Integer changed)->Status{auto n=organization_instance_sequence(id,prefix);const auto* h=common.state().facts.find(id);auto at=std::find(order.begin(),order.end(),id);if(!n||*n>=common.state().next_instance_id||!used.insert(*n).second||!h||h->id!=id||h->kind!=kind||h->domain!=domain||at==order.end()||changed<0||changed>common.tick())return common_failure("PoliticalSuccessorRoundPlayerSourceMismatch",id);const bool earlier=*n<*sequence;if(earlier!=(at<end)||(earlier&&changed>tick)||(!earlier&&changed<tick))return common_failure("PoliticalSuccessorRoundPlayerOrderMismatch",id);return common_success();};
 for(const auto& id:family.successions){auto source=family_succession_step(family,id);if(!source.ok)return {false,{},source.error};const auto* h=common.state().facts.find(id);if(!h||(h->kind!="family_succession"&&h->kind!="family_voluntary_succession"))return fail("PoliticalSuccessorRoundPlayerSourceMismatch",id);auto v=crossing(id,"family",h->kind,"family",source.value.tick);if(!v.ok)return {false,{},v.error};}
 if(legacy)for(const auto& row:legacy->activations.rows()){auto v=crossing(row.first,"family_legacy","family_legacy_control_activated","family_legacy",row.second.tick);if(!v.ok)return {false,{},v.error};}
 return legacy?family_player_before_fact_with_legacy(common,family,*legacy,tick,boundary):family_player_before_fact(common,family,tick,boundary);
}
}
