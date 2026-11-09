// 真实原道别的暂态来源，必须等两端原resume与唯一Interaction结果都完成。
#include "private_shop_travel_encounter_leave_source.hpp"
#include "resident/domains/travel/travel.hpp"
#include "resident/storage/common_codec.hpp"
#include <limits>
namespace hanmo::resident {
struct PrivateShopTravelEncounterLeaveBoundary::State {
 const CommonStore* common=nullptr;const TravelState* travel=nullptr;const InteractionState* interaction=nullptr;
 std::shared_ptr<const ResourceExecutionCaptureIdentity> capture;
 CommonState common_before;TravelState travel_before;EncounterRecord encounter;StableId actor;
 std::array<TravelOriginalPlanSnapshot,2> before;std::size_t recipe_begin=0;
};
// 只在本叶解开原值，调用者不能改换旧Encounter或恢复的暂停位。
struct PrivateShopTravelEncounterLeaveSourceAccess {static const auto& state(const PrivateShopTravelEncounterLeaveBoundary& b){return b.state_;}};
namespace {
bool same_location(const Location&a,const Location&b){return compare_boundary(boundary::encode_location(a),boundary::encode_location(b)).exact();}
bool same_point(PointI a,PointI b){return a.x==b.x&&a.y==b.y;}
bool same_points(const std::vector<PointI>&a,const std::vector<PointI>&b){if(a.size()!=b.size())return false;for(std::size_t i=0;i<a.size();++i)if(!same_point(a[i],b[i]))return false;return true;}
bool same_action(const ActionCoreView&a,const ActionCoreView&b){return a.id==b.id&&a.kind==b.kind&&a.actor_id==b.actor_id&&a.participants==b.participants&&a.start_tick==b.start_tick&&a.remaining_ticks==b.remaining_ticks;}
// 原导出包含字段存在性/顺序；另外核载荷内部id，不能只比JSON表键。
bool same_travel(const TravelState&a,const TravelState&b){
 // 即使原可选键未输出，其typed值/存在旗标和原字段序仍须不变，不能只比编码后的可见键。
 if(a.actions.size()!=b.actions.size()||a.road_actions.size()!=b.road_actions.size()||a.journey_facts.size()!=b.journey_facts.size()||a.itineraries.size()!=b.itineraries.size())return false;
 for(const auto& row:a.actions.rows()){const auto* v=b.actions.find(row.first);const auto& p=row.second;if(!v||p.has_encounter_id!=v->has_encounter_id||p.has_met_actions!=v->has_met_actions||p.encounter_id!=v->encounter_id||p.met_actions!=v->met_actions||p.field_order!=v->field_order)return false;}
 for(const auto& row:a.road_actions.rows()){const auto* v=b.road_actions.find(row.first);const auto& p=row.second;if(!v||p.has_terrain!=v->has_terrain||p.has_paused!=v->has_paused||p.has_reverse_first!=v->has_reverse_first||p.has_encounter_id!=v->has_encounter_id||p.has_met_actions!=v->has_met_actions||p.paused!=v->paused||p.reverse_first!=v->reverse_first||p.encounter_id!=v->encounter_id||p.met_actions!=v->met_actions||p.field_order!=v->field_order)return false;}
 for(const auto& row:a.journey_facts.rows()){const auto* v=b.journey_facts.find(row.first);if(!v||row.second.id!=v->id||row.second.field_order!=v->field_order)return false;}
 for(const auto& row:a.itineraries.rows()){const auto* v=b.itineraries.find(row.first);if(!v||row.second.field_order!=v->field_order)return false;}
 auto x=export_travel_state(a),y=export_travel_state(b);return x.ok&&y.ok&&compare_boundary(x.value,y.value).exact();}
bool same_snapshot(const TravelOriginalPlanSnapshot&a,const TravelOriginalPlanSnapshot&b){if(!same_action(a.action,b.action)||!same_location(a.location,b.location)||bool(a.params)!=bool(b.params)||bool(a.terrain_plan)!=bool(b.terrain_plan))return false;TravelState x,y;auto insert=[](TravelState& t,const TravelOriginalPlanSnapshot& s){if(s.params)std::visit([&](const auto&p){using T=std::decay_t<decltype(p)>;if constexpr(std::is_same_v<T,RoadTravelParams>)t.road_actions.insert(s.action.id,p);else t.actions.insert(s.action.id,p);},*s.params);if(s.terrain_plan)t.journey_facts.insert(s.terrain_plan->id,*s.terrain_plan);};insert(x,a);insert(y,b);return same_travel(x,y);}
bool same_encounter(const EncounterRecord&a,const EncounterRecord&b){return a.id==b.id&&a.leaders==b.leaders&&a.participants==b.participants&&same_location(a.anchors[0],b.anchors[0])&&same_location(a.anchors[1],b.anchors[1])&&a.anchor_field_orders==b.anchor_field_orders&&a.anchors_order==b.anchors_order&&a.action_ids_order==b.action_ids_order&&a.resume_paused_order==b.resume_paused_order&&a.action_ids==b.action_ids&&a.resume_paused==b.resume_paused&&a.created_tick==b.created_tick&&a.resolved_tick==b.resolved_tick&&a.status==b.status&&a.result_id==b.result_id&&a.talked==b.talked&&a.field_order==b.field_order;}

// 原EncounterLeave的报价只携带原encounter_id，不能借别的对白报价或结果混入。
bool original_quote(const InteractionQuote& q,const StableId& id){return q.duration==0&&q.encounter_id==id&&q.dialogue_id.empty()&&q.next.empty()&&q.event_id.empty()&&!q.context&&!q.source&&!q.party_id&&!q.target&&!q.cost_per_tick&&!q.regional_binding&&!q.news_quote&&!q.party_recruitment_consent;}
bool original_effect(const InteractionEffect& e,const EncounterRecord& record){return e.encounter_id==record.id&&e.settlement_result==record.result_id&&!e.party_id&&!e.person_id&&!e.news_ids;}
}
Outcome<PrivateShopTravelEncounterLeaveBoundary> capture_private_shop_travel_encounter_leave_boundary(const CommonStore& c,const TravelState& t,const InteractionState& s,const StableId& actor,const EncounterLeave& command,const InteractionQuote& quote){
 const auto fail=[](const StableId& code)->Outcome<PrivateShopTravelEncounterLeaveBoundary>{return {false,{},common_failure(code).error};};const auto token=c.resource_execution_capture_identity();const auto* person=c.actor(actor);const EncounterRecord* record=nullptr;for(const auto& e:s.encounters)if(e.id==command.encounter_id){if(record)return fail("PrivateShopTravelLeaveOriginalRequired");record=&e;}
 if(!token||!person||!person->alive||!person->available||command.encounter_id.empty()||!original_quote(quote,command.encounter_id)||!record||record->status!="pending"||record->resolved_tick!=-1||!record->result_id.empty()||record->created_tick<0||record->created_tick>c.tick()||std::find(record->participants.begin(),record->participants.end(),actor)==record->participants.end()||record->leaders[0]==record->leaders[1]||record->action_ids[0].empty()||record->action_ids[0]==record->action_ids[1]||c.state().next_instance_id<=0||c.state().next_instance_id==std::numeric_limits<Integer>::max())return fail("PrivateShopTravelLeaveOriginalRequired");
 auto b=std::make_shared<PrivateShopTravelEncounterLeaveBoundary::State>();b->common=&c;b->travel=&t;b->interaction=&s;b->capture=token;b->common_before=c.state();b->travel_before=t;b->encounter=*record;b->actor=actor;b->recipe_begin=c.resource_execution_drafts().size();
 for(std::size_t i=0;i<2;++i){const auto* leader=c.actor(record->leaders[i]);auto snapshot=capture_original_travel_plan_snapshot(c,t,record->action_ids[i]);if(!leader||leader->action_id!=record->action_ids[i]||!same_location(leader->location,record->anchors[i])||!snapshot.ok||!snapshot.value.params||snapshot.value.action.kind!="travel"||snapshot.value.action.actor_id!=record->leaders[i])return fail("PrivateShopTravelLeaveActionRequired");bool bound=std::visit([&](const auto&p){return p.encounter_id==record->id&&p.paused;},*snapshot.value.params);if(!bound)return fail("PrivateShopTravelLeaveActionRequired");b->before[i]=std::move(snapshot.value);}
 PrivateShopTravelEncounterLeaveBoundary out;out.state_=std::move(b);return {true,std::move(out),{}};
}
Outcome<PrivateShopTravelEncounterLeaveSource> read_private_shop_travel_encounter_leave(const CommonStore& c,const TravelState& t,const InteractionState& s,const PrivateShopTravelEncounterLeaveBoundary& boundary_value,const std::array<TravelEncounterResumeDraft,2>& actual,const InteractionEffect& effect){
 const auto fail=[](const StableId& code)->Outcome<PrivateShopTravelEncounterLeaveSource>{return {false,{},common_failure(code).error};};const auto& b=PrivateShopTravelEncounterLeaveSourceAccess::state(boundary_value);if(!b||b->common!=&c||b->travel!=&t||b->interaction!=&s||!b->capture||b->capture!=c.resource_execution_capture_identity()||b->recipe_begin!=c.resource_execution_drafts().size())return fail("PrivateShopTravelLeaveScopeMismatch");
 auto expected=b->encounter;expected.status="resolved";expected.resolved_tick=b->common_before.identity.tick;expected.result_id="result:"+std::to_string(b->common_before.next_instance_id);const EncounterRecord* resolved=nullptr;for(const auto&e:s.encounters)if(e.id==expected.id){if(resolved)return fail("PrivateShopTravelLeaveResolvedRequired");resolved=&e;}if(!resolved||!same_encounter(*resolved,expected)||!original_effect(effect,expected))return fail("PrivateShopTravelLeaveResolvedRequired");
 // 原道别删除同一Encounter的会话；若仍有未结束会话，不能把中途复行当完整结果。
 for(const auto& session:s.interactions)if(session.context.encounter_id&&*session.context.encounter_id==expected.id)return fail("PrivateShopTravelLeaveResolvedRequired");
 auto common=b->common_before;++common.next_instance_id;auto travel=b->travel_before;
 for(std::size_t i=0;i<2;++i){const auto& draft=actual[i];const auto& id=expected.action_ids[i];if(draft.source_tick!=b->common_before.identity.tick||draft.original_encounter_id!=expected.id||draft.original_resume_paused!=expected.resume_paused[i]||!same_snapshot(draft.before,b->before[i]))return fail("PrivateShopTravelLeaveResumeMismatch");
  if(auto* p=travel.road_actions.find(id)){p->encounter_id.clear();if(!p->has_encounter_id&&!p->field_order.empty())p->field_order.push_back("encounter_id");p->has_encounter_id=true;if(!p->has_paused&&!p->field_order.empty())p->field_order.push_back("paused");p->has_paused=true;p->paused=expected.resume_paused[i];}
  else if(auto* p=travel.actions.find(id)){p->encounter_id.clear();if(!p->has_encounter_id&&!p->field_order.empty())p->field_order.push_back("encounter_id");p->has_encounter_id=true;p->paused=expected.resume_paused[i];}
  else return fail("PrivateShopTravelLeaveActionRequired");
  auto after=capture_original_travel_plan_snapshot(c,t,id);if(!after.ok||!same_snapshot(after.value,draft.after))return fail("PrivateShopTravelLeaveResumeMismatch");
 }
 // 原Common编码可见字段仅多一个内部result分配，不把缓存details归于此瞬时读。
 if(!compare_boundary(encode_common_state(common),encode_common_state(c.state())).exact()||!same_travel(travel,t))return fail("PrivateShopTravelLeaveActualMismatch");return {true,{c.state().identity,b->actor,b->encounter,*resolved,actual,effect},{}};
}
}
