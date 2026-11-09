#include "resident/domains/military/military_model.hpp"
// 只由同一原陆战伤亡导致的旅行领队退出生成失败完成来源；普通到达与舟战来源不混用。
#include "military_journey_termination.hpp"
#include "military_commander.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
namespace {
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& row:rows)if(row.id==id)return &row;return nullptr;}
bool has(const std::vector<StableId>& rows,const StableId& id){return std::find(rows.begin(),rows.end(),id)!=rows.end();}
bool before(const CommonStore& c,const StableId& first,const StableId& last){const auto& order=c.state().facts.order();auto a=std::find(order.begin(),order.end(),first),b=std::find(order.begin(),order.end(),last);return a!=order.end()&&b!=order.end()&&a<b;}
bool header(const CommonStore& c,const StableId& id,const StableId& kind){const auto* fact=c.state().facts.find(id);return fact&&fact->id==id&&fact->domain=="military"&&fact->kind==kind;}
template<class T>Outcome<T> fail(const StableId& id){return {false,{},common_failure("MilitaryJourneyTerminationSourceMismatch",id).error};}
}
bool military_field_commander_profile_in_use(const MilitaryState& state,const MilitaryCatalog& catalog){for(const auto& battle:state.battles){const auto* field=find(catalog.battle_catalog.battlefields,battle.battlefield_id);if(field&&field->commander_losses)return true;}return false;}
Outcome<std::vector<MilitaryJourneyTermination>> query_military_journey_terminations(const CommonStore& c,const MilitaryState& m,const MilitaryCatalog& cat,const StableId& source){
 using Result=std::vector<MilitaryJourneyTermination>;const auto* casualty=find(m.casualty_facts,source);const auto* fact=casualty?find(m.battle_facts,casualty->battle_id):nullptr;const auto* battle=fact?find(m.battles,fact->session_id):nullptr;const auto* contact=battle?find(m.contact_facts,battle->source_id):nullptr;
 if(!casualty||!fact||!battle||!contact||!header(c,source,"military_casualties")||!header(c,fact->id,"military_battle")||!header(c,contact->id,"military_contact")||!before(c,contact->id,fact->id)||!before(c,fact->id,source)||fact->phase!="completed"||casualty->tick!=fact->tick||fact->tick<0||fact->tick>c.tick()||fact->contact_source_id!=contact->id||!contact->battle_id||*contact->battle_id!=battle->id||!has(battle->round_receipt_ids,source))return fail<Result>(source);
 const auto* board=find(cat.battle_catalog.battlefields,battle->battlefield_id);if(!board)return fail<Result>(source);if(!board->commander_losses)return {true,{}, {}};
 Result out;if(!contact->motion_sources)return {true,{}, {}};std::set<StableId> actions;
 for(const auto& change:casualty->actor_changes){if(!has({"dead","captured","deserted"},change.status))continue;const MilitaryMotionSource* motion=nullptr;for(const auto& row:*contact->motion_sources)if(row.first==change.army_id&&row.second.actor_id==change.actor_id){if(motion)return fail<Result>(source);motion=&row.second;}if(!motion||motion->stationary)continue;if(motion->action_id.empty()||motion->plan_id.empty()||c.action(motion->action_id)||!has(battle->participant_actor_ids,change.actor_id)||!actions.insert(motion->action_id).second)return fail<Result>(source);
  const auto* actor=c.actor(change.actor_id);if(!actor)return fail<Result>(source);if(change.status=="dead"&&(actor->alive||actor->available||actor->hp!=0||!actor->action_id.empty()))return fail<Result>(source);Integer outcomes=0;for(const auto& row:casualty->actor_changes)if(row.actor_id==change.actor_id)++outcomes;if(outcomes!=1)return fail<Result>(source);
  bool exact=false;for(const auto& u:battle->units){if(u.army_id!=change.army_id)continue;const MilitaryRosterReceipt* row=nullptr;for(const auto& r:casualty->units)if(r.unit_id==u.unit_id)row=&r;if(!row)continue;if(has(u.member_ids,change.actor_id)&&has(change.status=="dead"?row->dead:change.status=="captured"?row->captured:row->deserted,change.actor_id))exact=true;else if(change.status=="dead"&&u.commander_actor_id==change.actor_id&&military_commander_loss(u)==2)exact=true;}if(!exact)return fail<Result>(source);
  out.push_back({motion->action_id,change.actor_id,battle->id,source,change.status=="dead"?"MilitaryCommanderKilled":change.status=="captured"?"MilitaryCommanderCaptured":"MilitaryCommanderDeserted",fact->tick});
 }
 return {true,std::move(out),{}};
}
Outcome<std::optional<MilitaryJourneyTermination>> query_military_journey_termination(const CommonStore& c,const MilitaryState& m,const MilitaryCatalog& cat,const StableId& source,const StableId& action,const StableId& actor){auto read=query_military_journey_terminations(c,m,cat,source);if(!read.ok)return {false,{},read.error};for(const auto& row:read.value)if(row.action_id==action){if(row.actor_id!=actor)return fail<std::optional<MilitaryJourneyTermination>>(source);return {true,row,{}};}return {true,std::nullopt,{}};}
}
