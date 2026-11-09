// 复用原开店资格隔离夹具；实际采购、入库、归还仍走原writer，不冒自然许可或永久货运全史。
#define main private_shop_original_transaction_fixture_main
#include "private_shop_transaction_test.cpp"
#undef main
#include "resident/domains/commerce/freight/private_shop_stock_movement_source.hpp"
#include "resident/domains/commerce/private_shop_restock.hpp"
#include "resident/domains/commerce/private_shop_closing.hpp"
#include "resident/domains/growth/growth_internal.hpp"
#include "resident/domains/growth/growth_life_results.hpp"
#include "resident/domains/local_economy/local_economy_internal.hpp"
#include "resident/domains/tasks/shop_orders/task_shop_asset_responsibilities.hpp"
namespace {
// 原生活读取直接委托真实Growth载荷，测试不自行拼采购事实。
struct OriginalTrades final:GrowthLifeRead {
 const GrowthState& state;
 explicit OriginalTrades(const GrowthState& value):state(value){}
 std::optional<LifeActivityView> life_result(const StableId& id)const override{return growth_life_result(state,id);}
 std::vector<StableId> life_fact_ids(Integer a,Integer b)const override{return growth_life_fact_ids(state,a,b);}
};
// 仅隔离外域Task责任资格；原关闭财货和本域租债仍由原函数验证。
struct NoTaskAssets final:TaskPrivateShopAssetResponsibilityRead {
 const CommonStore& common;
 explicit NoTaskAssets(const CommonStore& value):common(value){}
 Outcome<TaskPrivateShopAssetResponsibilities> current(const TaskPrivateShopAssetScope& scope)const override{return {true,{common.state().identity,scope,common.tick(),{},{}},{}};}
 Outcome<TaskPrivateShopAssetResponsibilities> before_close(const TaskPrivateShopAssetScope& scope,const StableId& fact,Integer tick)const override{return {true,{common.state().identity,scope,tick,fact,{}},{}};}
};
template<class T>T need(Outcome<T> result,const char* label){if(!result.ok)throw std::runtime_error(std::string(label)+": "+result.error.original.code);check(true,label);return std::move(result.value);}
struct StockFixture:Fixture {
 OriginalTrades original{study_history};NoTaskAssets tasks{common};SocialState social;GrowthAccess growth_access;
 PrivateShopOpeningReceipt opening;StableId old_bag,trade,stock_fact;
 StockFixture(){
  opening=need(open_private_shop(common,economy,state,context(),meeting(),quote()),"real shop before stock movement");
  markets.shops.find("original.market")->representative_roles={"lessor.role"};
  check(common.apply_resources({{AddLot{"owner.bag","grain",2,{Owner::Kind::actor,"owner",{}},{}},AddLot{"lessor.bag","grain",3,{Owner::Kind::actor,"lessor",{}},{}},AddLot{"lessor.bag","grain",8,{Owner::Kind::actor,"lessor",{}},{}}},{}}).ok,"real old bag and distinct market batches");
  old_bag=common.inventory("owner.bag")->lots.front().id;
  growth_access.contact=[](const StableId&,const InteractionContext& value,const StableId&,const StableId&)->Outcome<InteractionContext>{return {true,value,{}};};
  growth_access.battle_occupied=[](const StableId&){return false;};
  check(common.begin_resource_execution_capture().ok,"capture only actual original resource executions");
 }
 PrivateShopContext business(){auto x=context();x.trades=&original;x.closing_tasks=&tasks;return x;}
 PrivateShopStockMovementContext movement(){return {common,state,catalog,&study_history};}
 PrivateShopRestockAdmission start_purchase(){
  const InteractionContext contact{"market","lessor","lessor.role"};
  auto admission=need(prepare_private_shop_restock(business(),study_history,social,growth_access,"owner",{opening.shop_id,"grain",5,contact}),"real original stocking quote");
  trade=need(common.next_id("action"),"original purchase action allocation");ActionMutation insert;insert.kind=ActionMutation::Kind::insert;insert.action={trade,"trade","owner",{"owner","lessor"},common.tick(),admission.trade.duration};check(common.apply_action(insert).ok,"actual purchase action");
  for(const auto& actor:{StableId{"owner"},StableId{"lessor"}}){ActorMutation occupy;occupy.actor_id=actor;occupy.action_id=trade;check(common.apply_actor(occupy).ok,"actual original purchase occupations");}
  auto begun=start_growth(common,study_history,markets,*common_catalog,social,growth_access,insert.action,admission.trade);if(!begun.ok)throw std::runtime_error("original purchase: "+begun.error.original.code);check(true,"original qualified Growth writer");return admission;
 }
 PrivateShopStockMovementBoundary receive(){
  auto admission=start_purchase();auto before=need(capture_private_shop_stock_movement_boundary(movement(),opening.shop_id,PrivateShopStockMovementKind::purchase_stocking),"boundary after real purchase before stock head");
  stock_fact=need(capture_private_shop_restock_in_candidate(common,state,business(),admission,trade),"actual original Adjust plus Add stocking writer");return before;
 }
 // 组件只执行原贸易完工及原Common释放序；不实例化World、军旅或导演推进。
 void finish_purchase(){
  auto action=*common.action(trade);action.remaining_ticks=0;ActionMutation progressed;progressed.kind=ActionMutation::Kind::patch;progressed.action=action;progressed.remaining_ticks=0;check(common.apply_action(progressed).ok,"original trade step reaches zero");
  need(growth::finish_service_trade(common,study_history,markets,social,conversations,action,nullptr),"actual original trade completion");
  check(common.apply_resources({{ReleaseActionResources{trade}}, {}}).ok,"original release resource recipe");
  for(const auto& actor:action.participants){ActorMutation released;released.actor_id=actor;released.action_id=StableId{};check(common.apply_actor(released).ok,"original participant release");}
  check(cancel_life_activity(common,study_history,trade,common.tick(),"ActionCancelled").ok,"completed life source is retained by original release");study_history.actions.erase(trade);ActionMutation erase;erase.kind=ActionMutation::Kind::erase;erase.action=action;check(common.apply_action(erase).ok,"original action release");
  ++common.state().identity.tick;check(validate_private_shops(business()).ok,"full original shop remains valid after true purchase completion");
 }
};
Outcome<StableId> actual_close(StockFixture& f,const PrivateShopClosingQuote& quote){local_economy_detail::PhaseSavepoint common(f.common);if(!common.token().ok)return {false,{},common.token().error};local_economy_detail::CandidateStateGuard<PrivateShopState> state(f.state);auto closed=close_private_shop_in_candidate(f.common,f.state,f.business(),quote);if(!closed.ok)return closed;auto commit=common.commit();if(!commit.ok)return {false,{},commit.error};state.commit();return closed;}
void actual_stocking(){
 StockFixture f;auto before=f.receive();const auto common=encode_common_state(f.common.state()),shops=encode_private_shop_state(f.state);auto source=need(read_private_shop_original_stock_movement(f.movement(),before,f.stock_fact),"read exact original stocking source");
 const auto& purchase=std::get<PrivateShopPurchaseFact>(source.original_record);check(source.kind==PrivateShopStockMovementKind::purchase_stocking&&source.original_trade_fact_id==f.trade&&purchase.purchased_lots.size()==2&&purchase.stocked_lots.size()==1,"two real bought fragments become one original stock lot");
 check(std::holds_alternative<AdjustLots>(source.actual_recipe.recipe.operations[0])&&std::holds_alternative<AddLot>(source.actual_recipe.recipe.operations[1]),"stocking does not invent a TransferLots identity");
 check(f.common.inventory("owner.bag")->lots.size()==1&&f.common.inventory("owner.bag")->lots.front().id==f.old_bag&&f.common.inventory("owner.bag")->lots.front().quantity==2,"old same-item private bag batch is untouched");
 check(compare_boundary(common,encode_common_state(f.common.state())).exact()&&compare_boundary(shops,encode_private_shop_state(f.state)).exact(),"pure stocking read allocates and writes nothing");
 auto bad=before;bad.original_common=nullptr;check(!read_private_shop_original_stock_movement(f.movement(),bad,f.stock_fact).ok,"detached common boundary rejected");bad=before;bad.original_shops=nullptr;check(!read_private_shop_original_stock_movement(f.movement(),bad,f.stock_fact).ok,"detached shop boundary rejected");bad=before;bad.kind=PrivateShopStockMovementKind::closing_return;check(!read_private_shop_original_stock_movement(f.movement(),bad,f.stock_fact).ok,"stocking cannot impersonate exit return");
 bad=before;++bad.next_instance_before;check(!read_private_shop_original_stock_movement(f.movement(),bad,f.stock_fact).ok,"stock head must be actual next original allocation");bad=before;++bad.versions[0].version;check(!read_private_shop_original_stock_movement(f.movement(),bad,f.stock_fact).ok,"wrong original inventory version rejected");
 check(f.common.apply_resources({{WalletDelta{f.opening.wallet_id,0,true}}, {}}).ok,"real interposed zero recipe");check(!read_private_shop_original_stock_movement(f.movement(),before,f.stock_fact).ok,"extra recipe cannot hide inside original stock movement");
}
void corrupted_stocking(){
 StockFixture f;auto before=f.receive();auto& draft=const_cast<ResourceExecutionDraft&>(f.common.resource_execution_drafts().back());const auto original=draft;auto* fact=f.state.purchases.find(f.stock_fact);const auto record=*fact;
 draft.operations[0].lot_flows.front().source_lot_id=f.old_bag;check(!read_private_shop_original_stock_movement(f.movement(),before,f.stock_fact).ok,"old same-item bag batch cannot replace true purchased fragment");draft=original;
 draft.operations[1].lot_flows.front().destination_lot_id=f.old_bag;check(!read_private_shop_original_stock_movement(f.movement(),before,f.stock_fact).ok,"new stock lot cannot reuse old bag identity");draft=original;
 std::get<AdjustLots>(draft.recipe.operations[0]).bump_revision=false;check(!read_private_shop_original_stock_movement(f.movement(),before,f.stock_fact).ok,"non-original no-revision adjustment rejected");draft=original;
 fact->purchase_cash++;check(!read_private_shop_original_stock_movement(f.movement(),before,f.stock_fact).ok,"stock cost remains actual original trade amount");*fact=record;
 fact->purchased_lots.pop_back();check(!read_private_shop_original_stock_movement(f.movement(),before,f.stock_fact).ok,"one missing purchased fragment rejected");*fact=record;
 draft.operations[0].inventories.front().after.lots.front().quantity++;check(!read_private_shop_original_stock_movement(f.movement(),before,f.stock_fact).ok,"old unrelated bag quantity cannot drift in stock source");draft=original;
 check(read_private_shop_original_stock_movement(f.movement(),before,f.stock_fact).ok,"restored exact original stocking source accepted");
}
void original_return(){
 StockFixture f;f.receive();f.finish_purchase();auto quote=need(preview_private_shop_closing(f.business(),"owner",{f.opening.shop_id}),"original remaining money and goods close quote");auto before=need(capture_private_shop_stock_movement_boundary(f.movement(),f.opening.shop_id,PrivateShopStockMovementKind::closing_return),"original close boundary before recipe");
 auto id=need(actual_close(f,quote),"original whole close writer");auto source=need(read_private_shop_original_stock_movement(f.movement(),before,id),"read original close return");const auto& closed=std::get<PrivateShopClosingRecord>(source.original_record);
 check(source.kind==PrivateShopStockMovementKind::closing_return&&source.original_trade_fact_id.empty()&&closed.returned_lots.size()==1&&closed.returned_cash==quote.returned_cash,"return keeps original close meaning and real cash");
 check(std::holds_alternative<TransferLots>(source.actual_recipe.recipe.operations[2])&&closed.returned_lots.front().original_lot_id==quote.stock_lots.front().id,"actual TransferLots source is original stocked lot");
 check(f.common.inventory("owner.bag")->lots.front().id==f.old_bag&&f.common.inventory("owner.bag")->lots.front().quantity==2&&f.common.inventory(f.opening.inventory_id)->lots.empty(),"close preserves old bag and empties original stock");
 auto& draft=const_cast<ResourceExecutionDraft&>(f.common.resource_execution_drafts().back());const auto saved=draft;draft.operations[2].lot_flows.front().destination_lot_id=f.old_bag;check(!read_private_shop_original_stock_movement(f.movement(),before,id).ok,"close cannot credit old bag batch as returned cargo");draft=saved;
 auto* record=f.state.closings.find(id);const auto old=*record;record->returned_cash++;check(!read_private_shop_original_stock_movement(f.movement(),before,id).ok,"returned cash cannot become extra income");*record=old;
 record->returned_lots.front().original_lot_id=f.old_bag;check(!read_private_shop_original_stock_movement(f.movement(),before,id).ok,"close must identify exact original stock batch");*record=old;
 check(read_private_shop_original_stock_movement(f.movement(),before,id).ok,"restored exact original return accepted");
}
void empty_return_and_late_failure(){
 StockFixture empty;auto q=need(preview_private_shop_closing(empty.business(),"owner",{empty.opening.shop_id}),"original empty shop close quote");auto before=need(capture_private_shop_stock_movement_boundary(empty.movement(),empty.opening.shop_id,PrivateShopStockMovementKind::closing_return),"empty shop exact close boundary");auto id=need(actual_close(empty,q),"original empty stock cash return");auto read=need(read_private_shop_original_stock_movement(empty.movement(),before,id),"empty stock still has actual two money legs");check(read.actual_recipe.operations.size()==2&&std::get<PrivateShopClosingRecord>(read.original_record).returned_lots.empty(),"empty close neither manufactures lot nor freight fee");
 StockFixture late;late.common.state().next_instance_id=std::numeric_limits<Integer>::max();auto quote=need(preview_private_shop_closing(late.business(),"owner",{late.opening.shop_id}),"max-ID original quote");auto boundary=need(capture_private_shop_stock_movement_boundary(late.movement(),late.opening.shop_id,PrivateShopStockMovementKind::closing_return),"max-ID close boundary");const auto bytes=encode_common_state(late.common.state()),shops=encode_private_shop_state(late.state);const auto count=late.common.resource_execution_drafts().size();auto failed=actual_close(late,quote);check(!failed.ok,"actual final close ID allocation fails after cash recipe");check(compare_boundary(bytes,encode_common_state(late.common.state())).exact()&&compare_boundary(shops,encode_private_shop_state(late.state)).exact()&&late.common.resource_execution_drafts().size()==count,"original savepoint restores money IDs state and temporary draft prefix");check(!read_private_shop_original_stock_movement(late.movement(),boundary,"private_shop_closed:"+std::to_string(std::numeric_limits<Integer>::max())).ok,"rolled-back cash recipe supplies no return source");
}
// 各实际writer前重新开捕获即使资源全值未变也须取新身份，不能沿用旧recipe前界。
void capture_lifecycle(){
 StockFixture stock;auto admission=stock.start_purchase();check(stock.common.finish_resource_execution_capture().ok,"finish purchase capture before stock boundary");check(!capture_private_shop_stock_movement_boundary(stock.movement(),stock.opening.shop_id,PrivateShopStockMovementKind::purchase_stocking).ok,"inactive stocking boundary refused");check(stock.common.begin_resource_execution_capture().ok,"start empty stock capture");auto old=need(capture_private_shop_stock_movement_boundary(stock.movement(),stock.opening.shop_id,PrivateShopStockMovementKind::purchase_stocking),"old stock identity");check(stock.common.finish_resource_execution_capture().ok,"finish original stock scope");check(stock.common.begin_resource_execution_capture().ok,"restart same-address stock capture");auto fresh=need(capture_private_shop_stock_movement_boundary(stock.movement(),stock.opening.shop_id,PrivateShopStockMovementKind::purchase_stocking),"new stock identity");auto id=need(capture_private_shop_restock_in_candidate(stock.common,stock.state,stock.business(),admission,stock.trade),"original stocking after capture restart");check(!read_private_shop_original_stock_movement(stock.movement(),old,id).ok,"old stock scope cannot consume actual new recipe");check(read_private_shop_original_stock_movement(stock.movement(),fresh,id).ok,"new stock scope reads original writer");
 StockFixture close;auto q=need(preview_private_shop_closing(close.business(),"owner",{close.opening.shop_id}),"original close quote for lifecycle");auto before=need(capture_private_shop_stock_movement_boundary(close.movement(),close.opening.shop_id,PrivateShopStockMovementKind::closing_return),"old close identity");CommonStore copied=close.common;check(!copied.resource_execution_capture_identity(),"copy clears closing capture");close.common=copied;check(close.common.begin_resource_execution_capture().ok,"same-address assigned Common begins new close capture");auto after=need(capture_private_shop_stock_movement_boundary(close.movement(),close.opening.shop_id,PrivateShopStockMovementKind::closing_return),"new close identity");auto done=need(actual_close(close,q),"actual original close after assignment");check(!read_private_shop_original_stock_movement(close.movement(),before,done).ok,"old closing identity rejected");check(read_private_shop_original_stock_movement(close.movement(),after,done).ok,"new closing identity remains readable");
}

}
int main(){try{actual_stocking();corrupted_stocking();original_return();empty_return_and_late_failure();capture_lifecycle();std::cout<<checks<<"/0 original stocking and return source checks\n";return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
