// 同次普通路遇安装后消费原Encounter及两端截停，不把回调草稿冒永久来源。
#include "private_shop_travel_contact_source.hpp"
#include "resident/domains/travel/travel.hpp"
#include "resident/storage/common_codec.hpp"
#include <cmath>
#include <cstring>
#include <limits>
#include <set>
namespace hanmo::resident {
struct PrivateShopTravelContactBoundary::State {
 const CommonStore* common=nullptr;const TravelState* travel=nullptr;const InteractionState* interaction=nullptr;
 std::shared_ptr<const ResourceExecutionCaptureIdentity> capture;
 CommonState original;TravelState original_travel;ContactHit hit;EncounterRecord expected;
 std::array<TravelOriginalPlanSnapshot,2> before;bool interactive=false;std::size_t recipe_begin=0;
};
// 外部只持不可变前界，不能替换原hit、人物/Action或伪造安装前值。
struct PrivateShopTravelContactSourceAccess {static const auto& state(const PrivateShopTravelContactBoundary& b){return b.state_;}};
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
// typed轨迹未进入持久JSON，逐字段核原值；不对含padding的整结构做memcmp。
bool same_number(double a,double b){return std::isfinite(a)&&std::isfinite(b)&&std::memcmp(&a,&b,sizeof(a))==0;}
bool same_contact_point(ContactPoint a,ContactPoint b){return same_number(a.x,b.x)&&same_number(a.y,b.y);}
bool same_params(const TerrainTravelParams&a,const TerrainTravelParams&b){TravelState x,y;x.actions.insert("action",a);y.actions.insert("action",b);return same_travel(x,y);}
bool same_params(const RoadTravelParams&a,const RoadTravelParams&b){TravelState x,y;x.road_actions.insert("action",a);y.road_actions.insert("action",b);return same_travel(x,y);}
bool same_motion(const TravelMotion&a,const TravelMotion&b){
 if(a.actor_id!=b.actor_id||a.action_id!=b.action_id||a.map_id!=b.map_id||a.origin!=b.origin||!same_params(a.params,b.params)||a.budget!=b.budget||a.end_credit!=b.end_credit||a.reached_points!=b.reached_points||!same_point(a.end,b.end)||a.moving!=b.moving||a.stationary!=b.stationary||bool(a.stationary_location)!=bool(b.stationary_location)||a.segments.size()!=b.segments.size()||a.read_versions.size()!=b.read_versions.size())return false;
 if(a.stationary_location&&!same_location(*a.stationary_location,*b.stationary_location))return false;
 for(std::size_t i=0;i<a.segments.size();++i){const auto&x=a.segments[i];const auto&y=b.segments[i];if(!same_contact_point(x.from,y.from)||!same_contact_point(x.to,y.to)||!same_number(x.start_cost,y.start_cost)||!same_number(x.end_cost,y.end_cost)||x.point_index!=y.point_index)return false;}
 for(std::size_t i=0;i<a.read_versions.size();++i){const auto&x=a.read_versions[i];const auto&y=b.read_versions[i];if(x.resource.kind!=y.resource.kind||x.resource.id!=y.resource.id||x.version!=y.version)return false;}return true;
}
bool same_motion(const RoadTravelMotion&a,const RoadTravelMotion&b){return a.actor_id==b.actor_id&&a.action_id==b.action_id&&a.edge_id==b.edge_id&&a.origin==b.origin&&a.start==b.start&&a.end==b.end&&a.moving==b.moving&&same_params(a.params,b.params);}
template<class E> bool same_endpoint(const E&a,const E&b){return a.actor_id==b.actor_id&&same_location(a.anchor,b.anchor)&&same_motion(a.trace,b.trace);}
bool same_hit(const ContactHit&a,const ContactHit&b){return a.a==b.a&&a.b==b.b&&a.time_num==b.time_num&&a.time_den==b.time_den&&same_endpoint(a.first,b.first)&&same_endpoint(a.second,b.second)&&bool(a.road)==bool(b.road)&&(!a.road||(same_endpoint(a.road->first,b.road->first)&&same_endpoint(a.road->second,b.road->second)));}
bool same_encounter(const EncounterRecord&a,const EncounterRecord&b){return a.id==b.id&&a.leaders==b.leaders&&a.participants==b.participants&&same_location(a.anchors[0],b.anchors[0])&&same_location(a.anchors[1],b.anchors[1])&&a.anchor_field_orders==b.anchor_field_orders&&a.anchors_order==b.anchors_order&&a.action_ids_order==b.action_ids_order&&a.resume_paused_order==b.resume_paused_order&&a.action_ids==b.action_ids&&a.resume_paused==b.resume_paused&&a.created_tick==b.created_tick&&a.resolved_tick==b.resolved_tick&&a.status==b.status&&a.result_id==b.result_id&&a.talked==b.talked&&a.field_order==b.field_order;}
// 仅匹配原选中命中的typed参与者/锚点，完整几何与ContactBatch版本仍由原检测链先验。
bool install_matches(const ContactHit& h,const EncounterInstall& i){
 if(h.a.empty()||h.b.empty()||h.a==h.b||h.time_den<=0||h.time_num<0||h.time_num>h.time_den||i.leaders!=std::array<StableId,2>{h.a,h.b}||!i.apply_motion)return false;
 if(h.road){const auto&a=h.road->first;const auto&b=h.road->second;return a.actor_id==h.a&&b.actor_id==h.b&&a.trace.actor_id==h.a&&b.trace.actor_id==h.b&&i.action_ids==std::array<StableId,2>{a.trace.action_id,b.trace.action_id}&&same_location(i.anchors[0],a.anchor)&&same_location(i.anchors[1],b.anchor)&&i.resume_paused==std::array<bool,2>{a.trace.params.paused,b.trace.params.paused};}
 const auto&a=h.first;const auto&b=h.second;return a.actor_id==h.a&&b.actor_id==h.b&&a.trace.actor_id==h.a&&b.trace.actor_id==h.b&&i.action_ids==std::array<StableId,2>{a.trace.action_id,b.trace.action_id}&&same_location(i.anchors[0],a.anchor)&&same_location(i.anchors[1],b.anchor)&&i.resume_paused==std::array<bool,2>{a.trace.params.paused,b.trace.params.paused};
}
}
Outcome<PrivateShopTravelContactBoundary> capture_private_shop_travel_contact_boundary(const CommonStore& c,const TravelState& t,const InteractionState& interaction,const ContactHit& hit,const EncounterInstall& install){
 const auto fail=[](const StableId& code)->Outcome<PrivateShopTravelContactBoundary>{return {false,{},common_failure(code).error};};const auto token=c.resource_execution_capture_identity();if(!token||!install_matches(hit,install)||c.tick()<0||c.tick()==std::numeric_limits<Integer>::max()||c.state().next_instance_id<=0||c.state().next_instance_id==std::numeric_limits<Integer>::max())return fail("PrivateShopTravelContactBoundaryRequired");
 auto b=std::make_shared<PrivateShopTravelContactBoundary::State>();b->common=&c;b->travel=&t;b->interaction=&interaction;b->capture=token;b->original=c.state();b->original_travel=t;b->hit=hit;b->recipe_begin=c.resource_execution_drafts().size();auto& e=b->expected;e.id="encounter:"+std::to_string(c.state().next_instance_id);e.leaders=install.leaders;e.anchors=install.anchors;e.action_ids=install.action_ids;e.resume_paused=install.resume_paused;e.anchor_field_orders=install.anchor_field_orders;e.anchors_order=install.anchors_order;e.action_ids_order=install.action_ids_order;e.resume_paused_order=install.resume_paused_order;
 if(e.action_ids[0].empty()||e.action_ids[0]==e.action_ids[1])return fail("PrivateShopTravelContactActionsRequired");
 for(std::size_t n=0;n<2;++n){const auto* actor=c.actor(e.leaders[n]);const auto* members=c.members(e.leaders[n]);auto snapshot=capture_original_travel_plan_snapshot(c,t,e.action_ids[n]);if(!actor||!members||!snapshot.ok||!snapshot.value.params||snapshot.value.action.kind!="travel"||snapshot.value.action.actor_id!=e.leaders[n]||actor->action_id!=e.action_ids[n]||bool(t.road_actions.find(e.action_ids[n]))!=bool(hit.road))return fail("PrivateShopTravelContactActionsRequired");b->before[n]=std::move(snapshot.value);e.participants.insert(e.participants.end(),members->begin(),members->end());}
 for(const auto& row:interaction.encounters)if(row.id==e.id)return fail("PrivateShopTravelContactOriginalCollision");
 b->interactive=std::find(e.participants.begin(),e.participants.end(),c.state().metadata.player_actor_id)!=e.participants.end();e.created_tick=c.tick()+1;e.resolved_tick=b->interactive?-1:e.created_tick;e.status=b->interactive?"pending":"resolved";if(!b->interactive&&c.state().next_instance_id==std::numeric_limits<Integer>::max()-1)return fail("PrivateShopTravelContactBoundaryRequired");if(!b->interactive)e.result_id="result:"+std::to_string(c.state().next_instance_id+1);
 PrivateShopTravelContactBoundary out;out.state_=std::move(b);return {true,std::move(out),{}};
}
Outcome<PrivateShopTravelContactSource> read_private_shop_travel_contact_installation(const CommonStore& c,const TravelState& t,const InteractionState& interaction,const PrivateShopTravelContactBoundary& boundary_value,const TravelContactTruncationDraft& actual){
 const auto fail=[](const StableId& code)->Outcome<PrivateShopTravelContactSource>{return {false,{},common_failure(code).error};};const auto& b=PrivateShopTravelContactSourceAccess::state(boundary_value);if(!b||b->common!=&c||b->travel!=&t||b->interaction!=&interaction||!b->capture||b->capture!=c.resource_execution_capture_identity()||b->recipe_begin!=c.resource_execution_drafts().size()||actual.source_tick!=b->original.identity.tick||actual.original_encounter_id!=b->expected.id||actual.interactive!=b->interactive||!same_hit(b->hit,actual.original_hit))return fail("PrivateShopTravelContactScopeMismatch");
 const EncounterRecord* record=nullptr;for(const auto& row:interaction.encounters)if(row.id==b->expected.id){if(record)return fail("PrivateShopTravelContactInstalledSourceRequired");record=&row;}if(!record||!same_encounter(*record,b->expected))return fail("PrivateShopTravelContactInstalledSourceRequired");
 auto expected=b->original;auto travel=b->original_travel;expected.next_instance_id+=b->interactive?1:2;
 for(std::size_t n=0;n<2;++n){if(!same_snapshot(b->before[n],actual.before[n]))return fail("PrivateShopTravelContactBeforeMismatch");auto* actor=expected.actors.find(b->expected.leaders[n]);const auto& action=b->expected.action_ids[n];
  if(b->hit.road){const auto& endpoint=n==0?b->hit.road->first:b->hit.road->second;auto p=endpoint.trace.params;if(!p.has_paused&&!p.field_order.empty())p.field_order.push_back("paused");p.has_paused=true;p.paused=b->interactive?true:endpoint.trace.params.paused;if(!p.has_encounter_id&&!p.field_order.empty())p.field_order.push_back("encounter_id");if(!p.has_met_actions&&!p.field_order.empty())p.field_order.push_back("met_actions");p.has_encounter_id=p.has_met_actions=true;p.encounter_id=b->interactive?record->id:StableId{};p.met_actions.push_back(b->expected.action_ids[1-n]);travel.road_actions.set(action,std::move(p));actor->location=endpoint.anchor;actor->location_order={"kind","edge_id","origin","progress"};}
  else{const auto& endpoint=n==0?b->hit.first:b->hit.second;auto p=endpoint.trace.params;p.paused=b->interactive?true:endpoint.trace.params.paused;p.encounter_id=b->interactive?record->id:StableId{};if(!p.has_encounter_id&&!p.field_order.empty())p.field_order.push_back("encounter_id");if(!p.has_met_actions&&!p.field_order.empty())p.field_order.push_back("met_actions");p.has_encounter_id=p.has_met_actions=true;p.met_actions.push_back(b->expected.action_ids[1-n]);travel.actions.set(action,std::move(p));actor->location=endpoint.anchor;actor->location_order.clear();}
  auto after=capture_original_travel_plan_snapshot(c,t,action);if(!after.ok||!same_snapshot(after.value,actual.after[n]))return fail("PrivateShopTravelContactAfterMismatch");
 }
 // 无资源recipe，Common仅原全局ID和两端位置改变。缓存details未在本编码中，非本临时口证明范围。
 if(!compare_boundary(encode_common_state(expected),encode_common_state(c.state())).exact()||!same_travel(travel,t))return fail("PrivateShopTravelContactActualMismatch");return {true,{c.state().identity,*record,actual},{}};
}
}
