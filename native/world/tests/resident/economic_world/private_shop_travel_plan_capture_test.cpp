// 原起行和途中控制的真实参数快照；不把同栈值当永久费用账或提交结果。
#define main private_shop_original_travel_boundary_main
#include "private_shop_travel_expense_source_test.cpp"
#undef main
#include "resident/domains/travel/travel_plan_capture.hpp"
#include "resident/storage/common_codec.hpp"
namespace {
// 与旧正式编码逐值比较，包含计划头/行动/字段存在性，不能只比观察DTO。
void plan_same_bytes(const Fixture& left,const Fixture& right){
    check(compare_boundary(encode_common_state(left.common.state()),encode_common_state(right.common.state())).exact(),"plan capture preserves exact original Common");
    auto a=export_travel_state(left.travel),b=export_travel_state(right.travel);require(a.ok&&b.ok,"original Travel encoded");check(compare_boundary(a.value,b.value).exact(),"plan capture preserves exact original Travel");
}
// 原报价/Action占用之后调用原begin，两个夹具只差是否请求同栈快照。
StableId actual_begin(Fixture& f,std::optional<TravelPlanInstallationDraft>* draft){
    TravelCommand command;command.destination=StableId{"B"};auto quote=plan_travel(f.common,f.travel,f.catalog,f.services,"carrier",command.destination);require(quote.ok,"original plan quote");
    auto id=f.common.next_id("action");require(id.ok,"actual action ID");ActionCoreView action;action.id=id.value;action.actor_id="carrier";action.kind="travel";action.participants={"carrier"};action.remaining_ticks=quote.value.duration;action.start_tick=f.common.tick();ActionMutation insert;insert.kind=ActionMutation::Kind::insert;insert.action=action;require(f.common.apply_action(insert).ok,"actual action inserted");ActorMutation occupy;occupy.actor_id="carrier";occupy.action_id=id.value;require(f.common.apply_actor(occupy).ok,"actual actor occupation");
    require(begin_travel(f.common,f.travel,f.catalog,id.value,quote.value,&command,draft).ok,"actual begin installs plan");return id.value;
}
// 道路没有伪journey头，地形保原一次新头；改道不覆盖已保存的前次原计划值。
void begin_and_control(bool terrain){
    Fixture captured,legacy;
    if(terrain)for(auto* f:{&captured,&legacy}){terrain::TerrainMap map;map.id="map";map.width=40;map.height=20;map.cell_size=20;map.cells.resize(2);map.budgets={{terrain::Mode::foot,10},{terrain::Mode::army,10},{terrain::Mode::boat,10}};map.money_per_tick=7;map.fatigue_per_tick=2;require(terrain::validate_map(map).ok,"actual terrain catalog valid");f->catalog.world_map_id=map.id;f->catalog.maps.insert(map.id,map);f->catalog.settlements.find("A")->map_xy={10,10};f->catalog.settlements.find("B")->map_xy={30,10};}
    std::optional<TravelPlanInstallationDraft> begun;auto id=actual_begin(captured,&begun);require(id==actual_begin(legacy,nullptr)&&begun.has_value(),"same actual begin IDs");plan_same_bytes(captured,legacy);
    check(!begun->before.params&&begun->after.params&&begun->original_command&&begun->original_quote.payer_wallet_id=="wallet.carrier","begin retains original quote and actual installed payload");check(begun->after.terrain_plan.has_value()==terrain,"only real terrain plan has original fact");check(captured.funds("carrier")==100,"begin does not prepay");
    for(const TravelCommandPayload command:{TravelCommandPayload{PauseTravelCommand{}},TravelCommandPayload{TravelWaitCommand{}},TravelCommandPayload{ResumeTravelCommand{}}}){
        auto prepared=prepare_travel_control(captured.common,captured.travel,captured.catalog,captured.services,"carrier",command);auto old=prepare_travel_control(legacy.common,legacy.travel,legacy.catalog,legacy.services,"carrier",command);require(prepared.ok&&old.ok,"original control prepared");std::optional<TravelControlDraft> controlled;const auto allocation=captured.common.state().next_instance_id;
        require(apply_travel_control(captured.common,captured.travel,captured.catalog,prepared.value,&controlled).ok&&apply_travel_control(legacy.common,legacy.travel,legacy.catalog,old.value).ok&&controlled.has_value(),"actual no-fee control output");plan_same_bytes(captured,legacy);check(controlled->before.action.id==id&&controlled->after.action.id==id&&controlled->original_control.command.index()==command.index(),"control records actual typed command and same action");check(captured.funds("carrier")==100&&captured.common.tick()==0&&captured.common.state().next_instance_id==allocation,"pause wait resume do not pay or advance");
    }
    require(step_travel(captured.common,captured.travel,captured.catalog,captured.services,id).ok&&step_travel(legacy.common,legacy.travel,legacy.catalog,legacy.services,id).ok,"original paid step before return");
    auto prepared=prepare_travel_control(captured.common,captured.travel,captured.catalog,captured.services,"carrier",ReturnTravelCommand{});auto old=prepare_travel_control(legacy.common,legacy.travel,legacy.catalog,legacy.services,"carrier",ReturnTravelCommand{});require(prepared.ok&&old.ok,"original return plan");std::optional<TravelControlDraft> controlled;
    require(apply_travel_control(captured.common,captured.travel,captured.catalog,prepared.value,&controlled).ok&&apply_travel_control(legacy.common,legacy.travel,legacy.catalog,old.value).ok&&controlled.has_value(),"actual return installs original new plan");plan_same_bytes(captured,legacy);check(captured.funds("carrier")==93&&controlled->original_control.quote.has_value(),"return quote no new charge or refund");
    if(terrain)check(controlled->before.terrain_plan->id!=controlled->after.terrain_plan->id&&begun->after.terrain_plan->id==controlled->before.terrain_plan->id,"old and new terrain original facts stay distinct");else check(!controlled->after.terrain_plan&&std::get<RoadTravelParams>(*controlled->after.params).returning,"road return does not invent original terrain fact");
    PreparedTravelOp invalid;invalid.actor_id="missing.actor";auto failed=apply_travel_control(captured.common,captured.travel,captured.catalog,invalid,&controlled);check(!failed.ok&&!controlled,"failed control clears previous draft");
}
// 真正原起点零距return在本次控制中落到起点，但行动释放与永久完成仍由中央负责。
void zero_distance_return(){
    Fixture f;std::optional<TravelPlanInstallationDraft> begin;auto id=actual_begin(f,&begin);auto op=prepare_travel_control(f.common,f.travel,f.catalog,f.services,"carrier",ReturnTravelCommand{});require(op.ok&&op.value.quote&&op.value.quote->duration==0,"real zero-distance return quote");const auto allocation=f.common.state().next_instance_id;std::optional<TravelControlDraft> draft;require(apply_travel_control(f.common,f.travel,f.catalog,op.value,&draft).ok&&draft.has_value(),"real zero-distance plan install");check(std::get<SettlementLocation>(draft->after.location).settlement_id=="A"&&f.common.action(id),"zero-distance actual place before central release");check(f.funds("carrier")==100&&f.common.state().next_instance_id==allocation,"zero-distance return neither pays nor allocates fact");
}
}
// 源用例须另取实际运行终态；本片仅原writer参数，不含完整World/运输报销。
int main(){try{begin_and_control(false);begin_and_control(true);zero_distance_return();}catch(const std::exception& e){check(false,e.what());}std::cout<<"TRAVEL_PLAN_CAPTURE_CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;}
