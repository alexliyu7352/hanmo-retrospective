// 隔离候选的原Travel实际收费捕获；复用真实Common/道路夹具，不替代永久来源或商铺资金验收。
#define main private_shop_original_travel_boundary_main
#include "private_shop_travel_expense_source_test.cpp"
#undef main
#include "resident/domains/travel/travel_step_expense.hpp"
#include "resident/storage/common_codec.hpp"
namespace {
// 比较原正式JSON编码的全部Common/Travel字节，不只比较钱数或草稿字段。
void same_original_state(const Fixture& with,const Fixture& without){
    auto left=render_boundary_json(encode_common_state(with.common.state()));auto right=render_boundary_json(encode_common_state(without.common.state()));
    require(left.ok&&right.ok,"original Common encoding succeeds");check(left.value==right.value,"capture and original no-output path Common bytes equal");
    auto a=export_travel_state(with.travel),b=export_travel_state(without.travel);require(a.ok&&b.ok,"original Travel encoding succeeds");
    auto x=render_boundary_json(a.value),y=render_boundary_json(b.value);require(x.ok&&y.ok,"original Travel JSON renders");check(x.value==y.value,"capture and original no-output path Travel bytes equal");
}
// 同样的正式起行及两步，输出草稿不能使旧付款、行动或载荷产生字节差异。
void road_capture_preserves_original_bytes(){
    Fixture with,without;auto a=with.begin("carrier","B"),b=without.begin("carrier","B");require(a==b,"same original road action IDs");
    for(int i=0;i<2;++i){std::optional<TravelStepExpenseDraft> draft;auto left=step_travel(with.common,with.travel,with.catalog,with.services,a,false,nullptr,&draft);auto right=step_travel(without.common,without.travel,without.catalog,without.services,b);require(left.ok&&right.ok&&draft.has_value(),"both actual road paths succeed");check(left.value.completed==right.value.completed,"same original road completion");same_original_state(with,without);++with.common.state().identity.tick;++without.common.state().identity.tick;}
}
// 真实地形目录经原plan/begin生成，不用调用者手报付款或伪造TerrainExecution。
void terrain_capture_preserves_original_bytes(){
    Fixture with,without;
    for(auto* f:{&with,&without}){terrain::TerrainMap map;map.id="map";map.width=40;map.height=20;map.cell_size=20;map.cells.resize(2);map.budgets={{terrain::Mode::foot,10},{terrain::Mode::army,10},{terrain::Mode::boat,10}};map.money_per_tick=7;map.fatigue_per_tick=2;require(terrain::validate_map(map).ok,"actual terrain map valid");f->catalog.world_map_id=map.id;f->catalog.maps.insert(map.id,map);f->catalog.settlements.find("A")->map_xy={10,10};f->catalog.settlements.find("B")->map_xy={30,10};}
    auto a=with.begin("carrier","B"),b=without.begin("carrier","B");require(a==b,"same original terrain action IDs");
    std::optional<TravelStepExpenseDraft> draft;auto left=step_travel(with.common,with.travel,with.catalog,with.services,a,true,nullptr,&draft);auto right=step_travel(without.common,without.travel,without.catalog,without.services,b,true);require(left.ok&&right.ok&&draft.has_value(),"both actual terrain paths succeed");
    check(draft->applied_payment&&draft->applied_payment->delta==-7&&draft->applied_payment->wallet_id=="wallet.carrier"&&draft->charged_road_id.empty()&&!draft->free_endpoint_finalization,"terrain captures original recipe once");
    check(draft->fatigue_writes.size()==1&&draft->fatigue_writes.front().actor_id=="carrier"&&draft->fatigue_writes.front().fatigue_before==0&&draft->fatigue_writes.front().fatigue_after==2&&draft->fatigue_writes.front().fatigue_per_tick==2&&draft->fatigue_writes.front().hp_before==draft->fatigue_writes.front().hp_after,"terrain publishes the original actual fatigue write");
    check(std::holds_alternative<TerrainTravelParams>(draft->before_params)&&std::holds_alternative<TerrainTravelParams>(draft->after_params)&&left.value.execution.has_value()==right.value.execution.has_value(),"terrain keeps original params and military execution shape");same_original_state(with,without);
}
// 草稿必须来自本次成功step；免费收尾与零价移动具有不同原资源操作。
void captured_road_charge(){
    Fixture f;auto id=f.begin("carrier","B");std::optional<TravelStepExpenseDraft> draft;
    const auto next=f.common.state().next_instance_id;
    auto paid=step_travel(f.common,f.travel,f.catalog,f.services,id,false,nullptr,&draft);
    require(paid.ok&&draft.has_value(),"real successful step returns draft");
    check(draft->action.id==id&&draft->action.actor_id=="carrier"&&draft->source_tick==0,"draft uses actual action and preclock tick");
    check(draft->applied_payment&&draft->applied_payment->wallet_id=="wallet.carrier"&&draft->applied_payment->delta==-7&&draft->applied_payment->require_spendable,"draft is exact applied original debit");
    check(draft->charged_road_id=="AB"&&!draft->free_endpoint_finalization&&!draft->completed,"draft identifies actual paid road");
    check(std::get<OnRouteLocation>(draft->before_location).progress==0&&std::get<OnRouteLocation>(draft->after_location).progress==100,"draft freezes real before and after motion");
    check(f.common.state().next_instance_id==next&&f.funds("carrier")==93,"capture allocates no source ID or additional payment");
    f.control("carrier",PauseTravelCommand{});
    auto denied=step_travel(f.common,f.travel,f.catalog,f.services,id,false,nullptr,&draft);
    check(!denied.ok&&!draft,"failed step clears previous successful caller draft");
}
// 原免费端点不执行资源recipe；正常零价移动仍保实际WalletDelta(0)。
void captured_free_and_zero(){
    Fixture f;auto id=f.begin("carrier","B");std::optional<TravelStepExpenseDraft> draft;
    f.common.state().actors.find("carrier")->location=OnRouteLocation{"AB","A",300};f.common.state().wallets.find("wallet.carrier")->balance=0;
    auto done=step_travel(f.common,f.travel,f.catalog,f.services,id,false,nullptr,&draft);
    require(done.ok&&draft.has_value(),"free endpoint draft exists");
    check(draft->free_endpoint_finalization&&draft->completed&&!draft->applied_payment&&draft->charged_road_id.empty(),"free endpoint has no fictitious payment");
    check(draft->fatigue_writes.empty(),"free endpoint has no fictitious condition write");
    Fixture z;z.catalog.roads.find("AB")->cost_per_tick=0;auto zero=z.begin("carrier","B");
    require(step_travel(z.common,z.travel,z.catalog,z.services,zero,false,nullptr,&draft).ok&&draft.has_value(),"zero price movement draft exists");
    check(!draft->free_endpoint_finalization&&draft->applied_payment&&draft->applied_payment->delta==0&&draft->charged_road_id=="AB","zero price movement retains actual zero debit operation");
    check(draft->fatigue_writes.size()==1&&draft->fatigue_writes.front().fatigue_after==2,"zero price movement still writes original fatigue");
}
// 到已达路段端点后收费的是真正下一边，不能拿step前边名或旧报价填草稿。
void captured_handoff_edge(){
    Fixture f;f.catalog.settlements.insert("C",{"C","C.exit",{500,0}});f.catalog.places.insert("C.exit",{"C",{}});
    f.catalog.roads.insert("BC",{"BC","B","C",2,19,1});auto id=f.begin("carrier","C");
    f.common.state().actors.find("carrier")->location=OnRouteLocation{"AB","A",300};std::optional<TravelStepExpenseDraft> draft;
    auto moved=step_travel(f.common,f.travel,f.catalog,f.services,id,false,nullptr,&draft);
    require(moved.ok&&draft.has_value(),"endpoint handoff step succeeds");
    check(draft->charged_road_id=="BC"&&draft->applied_payment&&draft->applied_payment->delta==-19&&f.funds("carrier")==81,"actual next edge charged once");
    check(draft->fatigue_writes.size()==1&&draft->fatigue_writes.front().fatigue_per_tick==1&&draft->fatigue_writes.front().fatigue_after==1,"handoff uses the actual newly charged edge fatigue");
    check(std::get<RoadTravelParams>(draft->before_params).edge_index==0&&std::get<RoadTravelParams>(draft->after_params).edge_index==1,"handoff retains both actual plan positions");
}
// 人为损坏下一边的低层故障夹具：已扣款后末尾拒绝不交出可发布草稿，原保存点负责全回滚。
void failed_after_debit_has_no_draft(){
    Fixture f;auto id=f.begin("carrier","B");f.common.state().actors.find("carrier")->location=OnRouteLocation{"AB","A",200};
    f.travel.road_actions.find(id)->path.push_back("missing.next");const auto saved=f.travel;require(f.common.begin_actor_condition_capture().ok,"late failure condition capture");auto token=f.common.begin_savepoint();require(token.ok,"original Common savepoint");
    std::optional<TravelStepExpenseDraft> draft;auto failed=step_travel(f.common,f.travel,f.catalog,f.services,id,false,nullptr,&draft);
    check(!failed.ok&&failed.error.original.code=="InvalidTravelPath"&&!draft,"late original failure emits no draft");
    check(f.funds("carrier")==93,"failure occurred after genuine original debit");
    require(f.common.rollback_savepoint(token.value).ok,"original rollback after late failure");f.travel=saved;
    check(f.funds("carrier")==100&&f.common.actor("carrier")->fatigue==0,"original money and fatigue restore together");
    check(f.common.actor_condition_execution_drafts().empty(),"late failed step condition drafts roll back with original body");
}
// 实际members原序和每次同值写保留；当前party只是低层输入，不冒永久成员资格。
void captured_condition_members(){
    Fixture f;f.common.state().actors.find("other")->location=SettlementLocation{"A","A.exit"};
    f.common.state().actors.find("other")->fatigue=99;f.common.state().actors.find("carrier")->fatigue=100;
    PartyRecord party;party.id="party.fixture";party.leader_id="carrier";party.members={"other","carrier"};f.common.state().parties.insert(party.id,party);
    auto id=f.begin("carrier","B");require(f.common.begin_actor_condition_capture().ok,"actual member condition capture");
    std::optional<TravelStepExpenseDraft> draft;auto step=step_travel(f.common,f.travel,f.catalog,f.services,id,false,nullptr,&draft);
    require(step.ok&&draft.has_value()&&draft->fatigue_writes.size()==2,"two true member writes");
    const auto& first=draft->fatigue_writes[0];const auto& last=draft->fatigue_writes[1];
    check(first.actor_id=="other"&&last.actor_id=="carrier","producer retains original members order");
    check(first.fatigue_before==99&&first.fatigue_after==100&&last.fatigue_before==100&&last.fatigue_after==100,"cap and same-value write are not dropped");
    const auto& actual=f.common.actor_condition_execution_drafts();require(actual.size()==2,"exact original Common condition legs");
    for(std::size_t i=0;i<actual.size();++i){const auto& a=actual[i];const auto& b=draft->fatigue_writes[i];check(a.actor_id==b.actor_id&&a.before&&a.before->hp==b.hp_before&&a.after.hp==b.hp_after&&a.before->fatigue==b.fatigue_before&&a.after.fatigue==b.fatigue_after&&!a.hp_written&&a.fatigue_written,"same exact Common before/after and original write presence");}
    Fixture z;z.catalog.roads.find("AB")->fatigue_per_tick=0;auto zid=z.begin("carrier","B");require(step_travel(z.common,z.travel,z.catalog,z.services,zid,false,nullptr,&draft).ok&&draft.has_value(),"zero fatigue successful movement");
    check(draft->fatigue_writes.size()==1&&draft->fatigue_writes.front().fatigue_per_tick==0&&draft->fatigue_writes.front().fatigue_before==draft->fatigue_writes.front().fatigue_after,"zero configured fatigue remains an actual same-value write");
}
}
// 只检查同栈producer草稿；永久费用段、原接触保留和批次连续性尚未启用。
int main(){try{free_endpoint();control_without_fees();actual_contact_keeps_paid_step();captured_road_charge();captured_free_and_zero();captured_handoff_edge();failed_after_debit_has_no_draft();road_capture_preserves_original_bytes();terrain_capture_preserves_original_bytes();captured_condition_members();}catch(const std::exception& e){check(false,e.what());}std::cout<<"TRAVEL_STEP_CAPTURE_CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;}
