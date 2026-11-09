// 原控制只写Travel的paused及其可选字段，钱货/Action/位置/钟和ID都保持原值。
#include "private_shop_travel_control_source.hpp"
#include "resident/domains/travel/travel.hpp"
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {
struct PrivateShopTravelControlBoundary::State {
 const CommonStore* common=nullptr;const TravelState* travel=nullptr;
 std::shared_ptr<const ResourceExecutionCaptureIdentity> capture;
 WorldIdentity identity;BoundaryValue common_before;TravelState travel_before;
 TravelOriginalPlanSnapshot snapshot;PreparedTravelOp command;std::size_t recipe_begin=0;
};
// 不可变前界仅在本叶解开，不接受调用者提交新的前像字段。
struct PrivateShopTravelControlAccess {static const auto& state(const PrivateShopTravelControlBoundary& b){return b.state_;}};
namespace {
bool no_fee(const PreparedTravelOp& op){return !op.quote&&(std::holds_alternative<PauseTravelCommand>(op.command)||std::holds_alternative<ResumeTravelCommand>(op.command)||std::holds_alternative<TravelWaitCommand>(op.command));}
// 原三类空载荷以variant精确区分；不能凭效果相同把wait改称pause。
bool same_command(const PreparedTravelOp&a,const PreparedTravelOp&b){return no_fee(a)&&no_fee(b)&&a.actor_id==b.actor_id&&a.command.index()==b.command.index();}
bool same_action(const ActionCoreView&a,const ActionCoreView&b){return a.id==b.id&&a.kind==b.kind&&a.actor_id==b.actor_id&&a.participants==b.participants&&a.start_tick==b.start_tick&&a.remaining_ticks==b.remaining_ticks;}
// 原编码同时核可选字段和顺序；实际payload内部id另核，避免JSON表键掩盖该字段。
bool same_travel(const TravelState&a,const TravelState&b){
 // 即使原可选键未输出，其typed值/存在旗标和原字段序仍须不变，不能只比编码后的可见键。
 if(a.actions.size()!=b.actions.size()||a.road_actions.size()!=b.road_actions.size()||a.journey_facts.size()!=b.journey_facts.size()||a.itineraries.size()!=b.itineraries.size())return false;
 for(const auto& row:a.actions.rows()){const auto* v=b.actions.find(row.first);const auto& p=row.second;if(!v||p.has_encounter_id!=v->has_encounter_id||p.has_met_actions!=v->has_met_actions||p.encounter_id!=v->encounter_id||p.met_actions!=v->met_actions||p.field_order!=v->field_order)return false;}
 for(const auto& row:a.road_actions.rows()){const auto* v=b.road_actions.find(row.first);const auto& p=row.second;if(!v||p.has_terrain!=v->has_terrain||p.has_paused!=v->has_paused||p.has_reverse_first!=v->has_reverse_first||p.has_encounter_id!=v->has_encounter_id||p.has_met_actions!=v->has_met_actions||p.paused!=v->paused||p.reverse_first!=v->reverse_first||p.encounter_id!=v->encounter_id||p.met_actions!=v->met_actions||p.field_order!=v->field_order)return false;}
 for(const auto& row:a.journey_facts.rows()){const auto* v=b.journey_facts.find(row.first);if(!v||row.second.id!=v->id||row.second.field_order!=v->field_order)return false;}
 for(const auto& row:a.itineraries.rows()){const auto* v=b.itineraries.find(row.first);if(!v||row.second.field_order!=v->field_order)return false;}
 auto x=export_travel_state(a),y=export_travel_state(b);return x.ok&&y.ok&&compare_boundary(x.value,y.value).exact();}
bool same_snapshot(const TravelOriginalPlanSnapshot&a,const TravelOriginalPlanSnapshot&b){if(!same_action(a.action,b.action)||!compare_boundary(boundary::encode_location(a.location),boundary::encode_location(b.location)).exact()||bool(a.params)!=bool(b.params)||bool(a.terrain_plan)!=bool(b.terrain_plan))return false;TravelState x,y;auto append=[](TravelState& s,const TravelOriginalPlanSnapshot& row){if(row.params)std::visit([&](const auto& p){using T=std::decay_t<decltype(p)>;if constexpr(std::is_same_v<T,RoadTravelParams>)s.road_actions.insert(row.action.id,p);else s.actions.insert(row.action.id,p);},*row.params);if(row.terrain_plan)s.journey_facts.insert(row.terrain_plan->id,*row.terrain_plan);};append(x,a);append(y,b);return same_travel(x,y);}
}
Outcome<PrivateShopTravelControlBoundary> capture_private_shop_travel_control_boundary(const CommonStore& c,const TravelState& t,const PreparedTravelOp& command){
 const auto fail=[&](const StableId& code)->Outcome<PrivateShopTravelControlBoundary>{return {false,{},common_failure(code,command.actor_id).error};};const auto token=c.resource_execution_capture_identity();const auto* actor=c.actor(command.actor_id);if(!token||!actor||!no_fee(command)||t.rescues.find(actor->action_id))return fail("PrivateShopTravelNoFeeControlBoundaryRequired");auto snapshot=capture_original_travel_plan_snapshot(c,t,actor->action_id);if(!snapshot.ok||!snapshot.value.params)return fail("PrivateShopTravelNoFeeControlActionRequired");auto b=std::make_shared<PrivateShopTravelControlBoundary::State>();b->common=&c;b->travel=&t;b->capture=token;b->identity=c.state().identity;b->common_before=encode_common_state(c.state());b->travel_before=t;b->snapshot=std::move(snapshot.value);b->command=command;b->recipe_begin=c.resource_execution_drafts().size();PrivateShopTravelControlBoundary out;out.state_=std::move(b);return {true,std::move(out),{}};
}
Outcome<PrivateShopTravelNoFeeControlSource> read_private_shop_travel_no_fee_control(const CommonStore& c,const TravelState& t,const PrivateShopTravelControlBoundary& boundary_value,const TravelControlDraft& actual){
 const auto fail=[](const StableId& code)->Outcome<PrivateShopTravelNoFeeControlSource>{return {false,{},common_failure(code).error};};const auto& b=PrivateShopTravelControlAccess::state(boundary_value);if(!b||b->common!=&c||b->travel!=&t||!b->capture||b->capture!=c.resource_execution_capture_identity()||b->recipe_begin!=c.resource_execution_drafts().size()||b->identity.tick!=actual.source_tick||!same_command(b->command,actual.original_control)||!same_snapshot(b->snapshot,actual.before)||!compare_boundary(b->common_before,encode_common_state(c.state())).exact())return fail("PrivateShopTravelNoFeeControlScopeMismatch");
 auto expected=b->travel_before;const bool pause=!std::holds_alternative<ResumeTravelCommand>(b->command.command);const auto& id=b->snapshot.action.id;if(auto* road=expected.road_actions.find(id)){if(!road->has_paused&&!road->field_order.empty())road->field_order.push_back("paused");road->has_paused=true;road->paused=pause;}else if(auto* terrain=expected.actions.find(id))terrain->paused=pause;else return fail("PrivateShopTravelNoFeeControlActionRequired");if(!same_travel(expected,t))return fail("PrivateShopTravelNoFeeControlActualMismatch");auto after=capture_original_travel_plan_snapshot(c,t,id);if(!after.ok||!same_snapshot(after.value,actual.after))return fail("PrivateShopTravelNoFeeControlActualMismatch");return {true,{b->identity,actual},{}};
}
}
