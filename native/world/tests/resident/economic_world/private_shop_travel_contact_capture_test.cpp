// 只测真实Travel接敌几何/截停/复行写者；原Interaction安装与永久pin另待完整组合。
#define main private_shop_original_travel_boundary_main
#include "private_shop_travel_expense_source_test.cpp"
#undef main
#include "resident/domains/travel/travel_contact_capture.hpp"
#include "resident/storage/common_codec.hpp"
namespace {
// 原双方真实先起行并收费，返回检测时未被截停的唯一命中。
ContactHit actual_contact(Fixture& f){
    auto other=f.begin("other","A");require(step_travel(f.common,f.travel,f.catalog,f.services,other).ok,"other original step");++f.common.state().identity.tick;
    auto carrier=f.begin("carrier","B");auto hits=detect_road_contacts(f.common,f.travel,f.catalog,f.services);require(hits.ok&&hits.value.size()==1,"actual original contact detected");
    require(step_travel(f.common,f.travel,f.catalog,f.services,carrier).ok&&step_travel(f.common,f.travel,f.catalog,f.services,other).ok,"both original paid steps before truncation");return hits.value.front();
}
// 与旧无输出路径逐值比较所有原Common及Travel数据，输出不能重付或额外分ID。
void exact_original_state(const Fixture& left,const Fixture& right){
    check(compare_boundary(encode_common_state(left.common.state()),encode_common_state(right.common.state())).exact(),"contact capture preserves exact Common state");auto a=export_travel_state(left.travel),b=export_travel_state(right.travel);require(a.ok&&b.ok,"original Travel encoding");check(compare_boundary(a.value,b.value).exact(),"contact capture preserves exact Travel state");
}
// Encounter号只作为原writer实际参数；本低层夹具不冒原Interaction事实已登记。
void actual_truncation_and_resume(){
    Fixture captured,legacy;auto hit=actual_contact(captured),old_hit=actual_contact(legacy);const auto money=captured.funds("carrier");const auto other_money=captured.funds("other");
    auto source=captured.common.next_id("encounter"),old_source=legacy.common.next_id("encounter");require(source.ok&&old_source.ok&&source.value==old_source.value,"original source ID allocation boundary");const auto allocation=captured.common.state().next_instance_id;
    std::optional<TravelContactTruncationDraft> draft;require(apply_contact_truncation(captured.common,captured.travel,hit,source.value,true,&draft).ok&&draft.has_value(),"actual successful truncation captured");require(apply_contact_truncation(legacy.common,legacy.travel,old_hit,old_source.value,true).ok,"old truncation succeeds");exact_original_state(captured,legacy);
    check(draft->original_encounter_id==source.value&&draft->source_tick==1&&draft->interactive&&draft->original_hit.time_num==hit.time_num&&draft->original_hit.time_den==hit.time_den,"capture owns original supplied geometry and exact source key");
    for(std::size_t i=0;i<2;++i){const auto& before=draft->before[i];const auto& after=draft->after[i];check(before.action.id==after.action.id&&before.action.remaining_ticks==after.action.remaining_ticks,"truncation keeps original action remaining");check(std::get<RoadTravelParams>(*after.params).encounter_id==source.value&&std::get<RoadTravelParams>(*after.params).paused,"actual installed contact payload captured");const auto* uncut=std::get_if<OnRouteLocation>(&before.location);check((uncut&&uncut->progress!=std::get<OnRouteLocation>(after.location).progress)||std::holds_alternative<SettlementLocation>(before.location),"snapshot distinguishes paid full step or reached endpoint from truncation");}
    check(captured.funds("carrier")==money&&captured.funds("other")==other_money&&captured.common.state().next_instance_id==allocation,"truncation adds no fee refund or source allocation");
    for(const auto* endpoint:{&hit.road->first,&hit.road->second}){const auto id=endpoint->trace.action_id;std::optional<TravelEncounterResumeDraft> resumed;require(resume_after_encounter(captured.common,captured.travel,id,endpoint->trace.params.paused,&resumed).ok&&resumed.has_value(),"actual original resume captured");require(resume_after_encounter(legacy.travel,id,endpoint->trace.params.paused).ok,"old original resume succeeds");check(resumed->original_encounter_id==source.value&&std::get<RoadTravelParams>(*resumed->before.params).encounter_id==source.value&&std::get<RoadTravelParams>(*resumed->after.params).encounter_id.empty(),"resume owns removed original contact key");check(resumed->before.action.remaining_ticks==resumed->after.action.remaining_ticks&&std::get<OnRouteLocation>(resumed->before.location).progress==std::get<OnRouteLocation>(resumed->after.location).progress,"resume does not replay paid motion");auto twice=resume_after_encounter(captured.common,captured.travel,id,false,&resumed);check(!twice.ok&&!resumed&&twice.error.original.code=="TravelResumeCaptureEncounterRequired","no second source from already cleared contact");}
    exact_original_state(captured,legacy);check(captured.funds("carrier")==money&&captured.funds("other")==other_money&&captured.common.state().next_instance_id==allocation,"resume does not pay or allocate");
    auto before=encode_common_state(captured.common.state());auto bad=hit;bad.road->second.trace.action_id=bad.road->first.trace.action_id;auto refused=apply_contact_truncation(captured.common,captured.travel,bad,source.value,true,&draft);check(!refused.ok&&!draft&&refused.error.original.code=="TravelContactCaptureOriginalSourceRequired","duplicate action source rejected and stale slot cleared");check(compare_boundary(before,encode_common_state(captured.common.state())).exact(),"bad capture boundary has no resource or position writes");
}
}
// 新接触草稿运行单独授权；不混同旧step150门或完整来源验收。
int main(){try{actual_truncation_and_resume();}catch(const std::exception& e){check(false,e.what());}std::cout<<"TRAVEL_CONTACT_CAPTURE_CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;}
