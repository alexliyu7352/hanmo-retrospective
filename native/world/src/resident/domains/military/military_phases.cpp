// 接触到未实现战役路径前明确返回fatal capability；没有相关对象时保原空领域行为。
#include "military_phases.hpp"
#include "military_field_command_source.hpp"
#include "battle_world.hpp"
#include "siege_world.hpp"
#include "military_lifecycle.hpp"
#include "resident/model/common_model.hpp"
#include <algorithm>
namespace hanmo::resident {
Status military_preflight(const CommonRead& c,const MilitaryState& s){
    if(!s.battles.empty())return military_capability_failure("preflight",s.battles.front().kind,s.battles.front().id);
    if(!s.sieges.empty())return military_capability_failure("preflight","siege",s.sieges.front().id);
    for(auto& id:c.action_order()){auto a=c.action(id);if(!a)continue;if(a->kind=="army_return"||a->kind=="army_resupply"||a->kind=="military_train")return military_capability_failure("preflight",a->kind,a->id);}
    return common_success();
}
Status military_siege_step(const MilitaryState& s){for(auto& siege:s.sieges)return military_capability_failure("siege_step","siege",siege.id);return common_success();}
Status military_battle_step(const MilitaryState& s){for(auto& battle:s.battles)return military_capability_failure("military_battle",battle.kind,battle.id);return common_success();}
Status military_battle_actor_changes(const MilitaryState& s,const std::vector<MilitaryActorChange>& changes){for(auto& change:changes)return military_capability_failure("battle_actor_changes",change.status,change.actor_id);for(auto& battle:s.battles)if(battle.phase!="closed")return military_capability_failure("battle_actor_changes",battle.kind,battle.id);return common_success();}
Status military_siege_battles_finished(const MilitaryState& s,const std::vector<StableId>& completed){for(auto& id:completed)return military_capability_failure("siege_battles_finished","battle_result",id);for(auto& siege:s.sieges)return military_capability_failure("siege_battles_finished","siege",siege.id);return common_success();}
Status military_siege_results(const MilitaryState& s,const std::vector<StableId>& results){for(auto& id:results)return military_capability_failure("siege_results","siege_result",id);for(auto& siege:s.sieges)return military_capability_failure("siege_results","siege",siege.id);return common_success();}
// 原函数仅撤销失效军令关联的战场/围城；没有这两类对象时不能虚构军团解散或资金补账。
Status release_invalid_military_commands(const CommonRead& c,const MilitaryState& s,Integer tick){
    if(tick<c.tick()||tick>c.tick()+1)return common_failure("MilitaryInvalidTick");
    for(auto& campaign:s.campaigns){if(campaign.army_id.empty())continue;const ArmyRecord* army=nullptr;for(auto& a:s.armies)if(a.id==campaign.army_id)army=&a;if(!army)return common_failure("CampaignArmyMismatch");if(army->status!="active")continue;auto actor=c.actor(campaign.actor_id);const OfficeRecord* office=nullptr;for(auto& o:s.offices)if(o.id==campaign.office_id)office=&o;if(!actor||!office)return common_failure("InvalidCampaignIdentity");bool invalid=!actor->alive||!actor->available||!office->enabled||office->holder_actor_id!=actor->id||(campaign.objective_result&&(campaign.objective_result->kind=="expired"||campaign.objective_result->kind=="cancelled"||campaign.objective_result->kind=="withdrawn"));if(!invalid)continue;for(auto& siege:s.sieges)if(siege.phase=="active")return military_capability_failure("release_invalid_commands","siege_termination",siege.id);for(auto& battle:s.battles)if(battle.phase!="closed")return military_capability_failure("release_invalid_commands","battle_termination",battle.id);}
    return common_success();
}
Status complete_military_returns(const MilitaryState& s,const std::vector<MilitaryReturnCompletion>& returns){for(auto& r:returns)return military_capability_failure("military_return_complete","army_return",r.order_id);for(auto& campaign:s.campaigns)if(campaign.status=="returning"||campaign.status=="awaiting_report")return military_capability_failure("military_return_complete",campaign.status,campaign.id);return common_success();}
Status begin_military_journeys(const MilitaryState& s,const std::vector<MilitaryJourneyRequest>& requests){for(auto& r:requests)return military_capability_failure("military_journeys","military_journey",r.army_id);for(auto& campaign:s.campaigns)if(campaign.status=="returning")return military_capability_failure("military_journeys",campaign.status,campaign.id);return common_success();}
// 完整军事阶段先核旧战场，再核围城计划；个人暂停由中央在此之前处理。
Status military_preflight(const CommonRead& c,const MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& a,Integer tick){auto b=preflight_military_battles(c,s,cat,a,tick);return b.ok?preflight_siege_world(c,s,cat,a):b;}
// 失权顺序保持原围城后野战；已落账伤亡不撤销，返回请求仍由既有军令状态协调。
Status release_invalid_military_commands(CommonWrite& c,MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& a,Integer tick){std::vector<std::pair<StableId,StableId>> reasons;for(auto& p:s.campaigns){if(p.army_id.empty())continue;auto army=std::find_if(s.armies.begin(),s.armies.end(),[&](auto& x){return x.id==p.army_id;});if(army==s.armies.end()||army->status!="active")continue;const auto reason=military_campaign_battle_stop_reason(c,s,cat,p);if(!reason.empty())reasons.emplace_back(p.army_id,reason);}auto reason_for=[&](const StableId& army){for(auto& r:reasons)if(r.first==army)return r.second;return StableId{};};for(auto& siege:s.sieges){if(siege.phase!="active")continue;auto campaign=std::find_if(s.campaigns.begin(),s.campaigns.end(),[&](auto& x){return x.id==siege.campaign_id;});if(campaign==s.campaigns.end())return common_failure("InvalidSiegeState");auto reason=reason_for(campaign->army_id);if(reason.empty())continue;auto stopped=terminate_siege_world(c,s,cat,a,siege.id,reason,tick);if(!stopped.ok)return {false,{},stopped.error};auto r=apply_military_actor_changes_from_sources(c,s,cat,a,stopped.value.actor_changes,stopped.value.casualty_source_ids);if(!r.ok)return r;}for(auto& battle:s.battles){if(battle.phase=="closed")continue;for(auto& army:battle.army_ids){auto reason=reason_for(army);if(reason.empty())continue;auto stopped=terminate_military_battle(c,s,cat,a,battle.id,reason,tick);if(!stopped.ok)return {false,{},stopped.error};auto r=apply_military_actor_changes_from_sources(c,s,cat,a,stopped.value.actor_changes,stopped.value.casualty_source_ids);if(!r.ok)return r;r=finish_siege_battles(c,s,cat,a,stopped.value.completed_battle_ids,tick);if(!r.ok)return r;break;}}return common_success();}
// 归营确认只能消费本次实际成功卸装的永久回执；重复确认复用军令幂等检查。
Status complete_military_returns(CommonWrite& c,MilitaryState& s,const MilitaryCatalog& cat,const MilitaryAccess& a,const std::vector<MilitaryReturnCompletion>& returns){for(auto& r:returns){auto done=acknowledge_military_return(c,s,cat,a,r.order_id,r.receipt_id);if(!done.ok)return done;}return common_success();}

}
