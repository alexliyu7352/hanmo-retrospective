// 用真实Common捕获与原Travel写者匹配同次付款；不替永久运费、资本化或World验收。
#define main private_shop_original_travel_boundary_main
#include "private_shop_travel_expense_source_test.cpp"
#undef main
#include "resident/domains/commerce/freight/private_shop_travel_payment_source.hpp"
#include "resident/storage/common_codec.hpp"
namespace {
// 该准备器只执行正式step；成功草稿与Common真实执行缓冲分别来自两个原写点。
TravelStepExpenseDraft actual_step(Fixture& f,const StableId& id){
    std::optional<TravelStepExpenseDraft> step;auto result=step_travel(f.common,f.travel,f.catalog,f.services,id,false,nullptr,&step);
    require(result.ok&&step.has_value(),"original successful step and actual Travel draft");return *step;
}
// 可有前面别人的真实recipe，但本次前界以后只容许这一笔；读口自身无资产/ID写。
void exact_paid_scope(){
    Fixture f;auto id=f.begin("carrier","B");require(f.common.begin_resource_execution_capture().ok,"real Common capture starts");
    require(f.common.apply_resources({{WalletDelta{"wallet.other",-2,true}}, {}}).ok,"unrelated recipe precedes scope");
    auto boundary=capture_private_shop_travel_payment_boundary(f.common,f.travel,id);require(boundary.ok&&boundary.value.recipe_begin==1,"actual boundary excludes prior recipe");
    auto step=actual_step(f,id);const auto original=encode_common_state(f.common.state());
    auto read=read_private_shop_travel_step_payment(f.common,boundary.value,step);require(read.ok&&read.value.actual_payment.has_value(),"actual payer debit maps exactly");
    check(read.value.paid_amount==7&&read.value.payer_wallet_id=="wallet.carrier"&&read.value.actual_payment->before->balance==100&&read.value.actual_payment->after.balance==93,"mapped amount and full payer values are actual");
    check(compare_boundary(original,encode_common_state(f.common.state())).exact(),"payment read leaves original Common bytes unchanged");
    auto bad=step;bad.applied_payment->delta=-6;check(!read_private_shop_travel_step_payment(f.common,boundary.value,bad).ok,"Travel amount cannot replace actual debit");
    bad=step;std::get<RoadTravelParams>(bad.after_params).payer_wallet_id="wallet.other";check(!read_private_shop_travel_step_payment(f.common,boundary.value,bad).ok,"after payer switch refused");
    auto wrong=boundary.value;++wrong.identity.execution_epoch;check(!read_private_shop_travel_step_payment(f.common,wrong,step).ok,"different candidate epoch refused");
    wrong=boundary.value;++wrong.recipe_begin;check(!read_private_shop_travel_step_payment(f.common,wrong,step).ok,"missing original recipe refused");
    require(f.common.apply_resources({{WalletDelta{"wallet.other",-1,true}}, {}}).ok,"real interposed extra recipe");check(!read_private_shop_travel_step_payment(f.common,boundary.value,step).ok,"even unrelated extra recipe cannot join this step scope");
}
// 免费收尾确实没有recipe；零价实际移动仍有一个真实零操作，不能互相冒充。
void free_and_zero_sources(){
    Fixture free;auto id=free.begin("carrier","B");free.common.state().actors.find("carrier")->location=OnRouteLocation{"AB","A",300};free.common.state().wallets.find("wallet.carrier")->balance=0;
    require(free.common.begin_resource_execution_capture().ok,"free endpoint capture");auto boundary=capture_private_shop_travel_payment_boundary(free.common,free.travel,id);require(boundary.ok,"free endpoint payer boundary");auto step=actual_step(free,id);auto read=read_private_shop_travel_step_payment(free.common,boundary.value,step);
    check(read.ok&&read.value.free_endpoint_finalization&&!read.value.actual_payment&&read.value.paid_amount==0&&free.common.resource_execution_drafts().empty(),"free endpoint is zero original recipes");
    auto false_paid=step;false_paid.free_endpoint_finalization=false;false_paid.applied_payment=WalletDelta{"wallet.carrier",0,true};check(!read_private_shop_travel_step_payment(free.common,boundary.value,false_paid).ok,"free endpoint cannot invent zero payment operation");
    Fixture zero;zero.catalog.roads.find("AB")->cost_per_tick=0;auto zid=zero.begin("carrier","B");require(zero.common.begin_resource_execution_capture().ok,"zero priced capture");auto zb=capture_private_shop_travel_payment_boundary(zero.common,zero.travel,zid);require(zb.ok,"zero priced boundary");auto zs=actual_step(zero,zid);auto zr=read_private_shop_travel_step_payment(zero.common,zb.value,zs);check(zr.ok&&!zr.value.free_endpoint_finalization&&zr.value.actual_payment&&zr.value.paid_amount==0&&zero.common.resource_execution_drafts().size()==1,"zero price movement retains original operation");
    zs.free_endpoint_finalization=true;zs.completed=true;zs.applied_payment.reset();zs.charged_road_id.clear();check(!read_private_shop_travel_step_payment(zero.common,zb.value,zs).ok,"zero priced operation cannot disappear into free endpoint");
}
// 针对性坏草稿夹具只篡改暂态副本字段，逐项核其不能覆盖真实原款/完整预约/分配序。
void malformed_actual_frames(){
    Fixture f;auto id=f.begin("carrier","B");require(f.common.apply_resources({{SetReservation{"wallet.carrier",ReservationRef{id},3,false}}, {}}).ok,"real prior action reservation");require(f.common.begin_resource_execution_capture().ok,"reserved wallet capture");auto before=capture_private_shop_travel_payment_boundary(f.common,f.travel,id);require(before.ok,"reserved payer boundary");auto step=actual_step(f,id);require(read_private_shop_travel_step_payment(f.common,before.value,step).ok,"original reservation unchanged by step");
    // Common本体非const，此反例有意损坏它的临时草稿；产品只持const读取口。
    auto& record=const_cast<ResourceExecutionDraft&>(f.common.resource_execution_drafts().back());const auto saved=record;
    record.operations.front().wallets.front().after.reserved.clear();check(!read_private_shop_travel_step_payment(f.common,before.value,step).ok,"reservation omission refused");record=saved;
    record.operations.front().wallets.front().before->owner.id="other";check(!read_private_shop_travel_step_payment(f.common,before.value,step).ok,"before ownership substitution refused");record=saved;
    record.operations.front().inventories.push_back({});check(!read_private_shop_travel_step_payment(f.common,before.value,step).ok,"unrelated inventory mutation refused");record=saved;
    record.operations.front().lot_flows.push_back({});check(!read_private_shop_travel_step_payment(f.common,before.value,step).ok,"unexpected lot mapping refused");record=saved;
    record.operations.front().operation_index=1;check(!read_private_shop_travel_step_payment(f.common,before.value,step).ok,"wrong primitive position refused");record=saved;
    ++record.next_instance_after;check(!read_private_shop_travel_step_payment(f.common,before.value,step).ok,"hidden instance allocation refused");record=saved;
    record.recipe.operations.front()=SetReservation{"wallet.carrier",id,7,false};check(!read_private_shop_travel_step_payment(f.common,before.value,step).ok,"reservation cannot impersonate paid debit");record=saved;
    ++f.common.state().identity.tick;check(!read_private_shop_travel_step_payment(f.common,before.value,step).ok,"postclock delayed matching refused");
}
// 实际付款后的原末尾失败没有成功Travel草稿；原保存点同时恢复钱包与临时recipe前界。
void late_failure_rollback(){
    Fixture f;auto id=f.begin("carrier","B");f.common.state().actors.find("carrier")->location=OnRouteLocation{"AB","A",200};f.travel.road_actions.find(id)->path.push_back("missing.next");
    require(f.common.begin_resource_execution_capture().ok,"late failure capture begins");auto boundary=capture_private_shop_travel_payment_boundary(f.common,f.travel,id);require(boundary.ok,"late failure actual before");auto original=encode_common_state(f.common.state());auto point=f.common.begin_savepoint();require(point.ok,"actual original savepoint");
    std::optional<TravelStepExpenseDraft> step;auto failed=step_travel(f.common,f.travel,f.catalog,f.services,id,false,nullptr,&step);check(!failed.ok&&!step&&f.funds("carrier")==93&&f.common.resource_execution_drafts().size()==1,"genuine debit before late failure has no successful step source");
    require(f.common.rollback_savepoint(point.value).ok,"original savepoint restores capture and money");check(f.common.resource_execution_drafts().empty()&&compare_boundary(original,encode_common_state(f.common.state())).exact(),"late failure restores exact Common and draft prefix");
}
// 免费收尾没有资源行也要保持同次身份；同址重开、原copy赋值均不能复用旧before。
void capture_lifecycle(){
 for(bool free:{false,true})for(bool assignment:{false,true}){Fixture f;auto id=f.begin("carrier","B");if(free){f.common.state().actors.find("carrier")->location=OnRouteLocation{"AB","A",300};f.common.state().wallets.find("wallet.carrier")->balance=0;}
  check(!capture_private_shop_travel_payment_boundary(f.common,f.travel,id).ok,"inactive payment capture rejected");require(f.common.begin_resource_execution_capture().ok,"begin payment identity");auto old=capture_private_shop_travel_payment_boundary(f.common,f.travel,id);require(old.ok,"old payment boundary");
  if(assignment){CommonStore copy=f.common;check(!copy.resource_execution_capture_identity(),"payment copy has no capture identity");f.common=copy;}else require(f.common.finish_resource_execution_capture().ok,"finish payment capture without writes");
  require(f.common.begin_resource_execution_capture().ok,"same-address new payment capture");auto fresh=capture_private_shop_travel_payment_boundary(f.common,f.travel,id);require(fresh.ok,"new actual payment boundary");auto step=actual_step(f,id);check(!read_private_shop_travel_step_payment(f.common,old.value,step).ok,"old payment scope cannot consume later matching paid or free step");check(read_private_shop_travel_step_payment(f.common,fresh.value,step).ok,"new payment identity matches original writer");
 }
}

}
int main(){try{exact_paid_scope();free_and_zero_sources();malformed_actual_frames();late_failure_rollback();capture_lifecycle();}catch(const std::exception& e){check(false,e.what());}std::cout<<"TRAVEL_PAYMENT_SOURCE_CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;}
