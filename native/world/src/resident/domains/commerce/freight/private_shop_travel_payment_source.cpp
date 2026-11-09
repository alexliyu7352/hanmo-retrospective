// 只消费真实同栈观察：钱包前后逐字段相等，不能凭净额或今日余额追认旧旅行付款。
#include "private_shop_travel_payment_source.hpp"
#include "resident/storage/common_store.hpp"
#include <limits>
#include <type_traits>
namespace hanmo::resident {namespace {
// 世界、代次、修订和逻辑钟都须保持在原step栈；半日提交以后不能重新匹配。
bool same_identity(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
// 原Action的全部进入值必须吻合成功草稿；结束后的remaining另由原Travel writer决定。
bool same_action(const ActionCoreView& a,const ActionCoreView& b){return a.id==b.id&&a.kind==b.kind&&a.actor_id==b.actor_id&&a.participants==b.participants&&a.start_tick==b.start_tick&&a.remaining_ticks==b.remaining_ticks;}
// 同栈拷贝的兼容元数据保同一只读对象；此处不接受解码后另造的等额付款记录。
bool same_wallet(const WalletView& a,const WalletView& b){
    if(a.id!=b.id||a.balance!=b.balance||!owner_equal(a.owner,b.owner)||a.owner.extra!=b.owner.extra||a.owner.field_order!=b.owner.field_order||a.field_order!=b.field_order||a.reserved.size()!=b.reserved.size())return false;
    for(std::size_t i=0;i<a.reserved.size();++i)if(a.reserved[i].action_id!=b.reserved[i].action_id||a.reserved[i].amount!=b.reserved[i].amount)return false;
    return true;
}
// 只对真实位置值作等值，不从位置推断距离、收费或到达权。
bool same_location(const Location &a, const Location &b) {
  if (a.index() != b.index())
    return false;
  return std::visit(
      [&](const auto &x) {
        using T = std::decay_t<decltype(x)>;
        const auto &y = std::get<T>(b);
        if constexpr (std::is_same_v<T, SettlementLocation>)
          return x.settlement_id == y.settlement_id && x.place_id == y.place_id;
        else if constexpr (std::is_same_v<T, WildernessLocation>)
          return x.map_id == y.map_id && x.origin == y.origin &&
                 x.point.x == y.point.x && x.point.y == y.point.y;
        else if constexpr (std::is_same_v<T, FollowingLocation>)
          return x.party_id == y.party_id;
        else if constexpr (std::is_same_v<T, InArmyLocation>)
          return x.army_id == y.army_id;
        else if constexpr (std::is_same_v<T, CarriedLocation>)
          return x.carrier_kind == y.carrier_kind &&
                 x.carrier_id == y.carrier_id;
        else
          return x.edge_id == y.edge_id && x.origin == y.origin &&
                 x.progress == y.progress;
      },
      a);
}
}
Outcome<PrivateShopTravelPaymentBoundary> capture_private_shop_travel_payment_boundary(const CommonStore& common,const TravelState& travel,const StableId& id){
    const auto fail=[&](const StableId& code)->Outcome<PrivateShopTravelPaymentBoundary>{return {false,{},common_failure(code,id).error};};
    const auto* action=common.action(id);const auto* actor=action?common.actor(action->actor_id):nullptr;
    const auto* terrain=travel.actions.find(id);const auto* road=travel.road_actions.find(id);
    if(!common.resource_execution_capture_identity()||!action||action->id!=id||action->kind!="travel"||!actor||actor->action_id!=id||action->start_tick<0||action->start_tick>common.tick()||bool(terrain)==bool(road)||travel.rescues.find(id))return fail("PrivateShopTravelPaymentBoundaryRequired");
    const auto& payer=terrain?terrain->payer_wallet_id:road->payer_wallet_id;const auto* wallet=common.wallet(payer);
    if(payer.empty()||!wallet||wallet->id!=payer)return fail("PrivateShopTravelPaymentPayerRequired");
    return {true,{common.state().identity,*action,*wallet,common.state().next_instance_id,common.resource_execution_drafts().size(),&common,common.resource_execution_capture_identity()},{}};
}
Outcome<PrivateShopTravelStepPayment> read_private_shop_travel_step_payment(const CommonStore& common,const PrivateShopTravelPaymentBoundary& before,const TravelStepExpenseDraft& step){
    const auto fail=[&](const StableId& code)->Outcome<PrivateShopTravelStepPayment>{return {false,{},common_failure(code,step.action.id).error};};
    const auto& drafts=common.resource_execution_drafts();const auto* action=common.action(before.action.id);const auto* actor=common.actor(before.action.actor_id);const auto* wallet=common.wallet(before.payer_before.id);
    if(before.original_common!=&common||!before.capture_identity||before.capture_identity!=common.resource_execution_capture_identity()||!same_identity(before.identity,common.state().identity)||before.identity.tick!=step.source_tick||before.next_instance_id<=0||before.next_instance_id!=common.state().next_instance_id||before.recipe_begin>drafts.size()||!same_action(before.action,step.action)||step.action.kind!="travel"||!action||action->kind!="travel"||action->actor_id!=step.action.actor_id||action->participants!=step.action.participants||action->start_tick!=step.action.start_tick||!actor||actor->action_id!=step.action.id||!same_location(actor->location,step.after_location)||!wallet)return fail("PrivateShopTravelPaymentScopeMismatch");
    if(step.before_params.index()!=step.after_params.index()||std::visit([](const auto& p){return p.payer_wallet_id;},step.before_params)!=before.payer_before.id||std::visit([](const auto& p){return p.payer_wallet_id;},step.after_params)!=before.payer_before.id)return fail("PrivateShopTravelPaymentPayerMismatch");
    PrivateShopTravelStepPayment out{before.identity,step.action.id,step.action.actor_id,before.payer_before.id,0,step.free_endpoint_finalization,{}};
    if(step.free_endpoint_finalization){
        if(!std::holds_alternative<RoadTravelParams>(step.before_params)||!step.completed||step.applied_payment||!step.charged_road_id.empty()||drafts.size()!=before.recipe_begin||!same_wallet(before.payer_before,*wallet))return fail("PrivateShopTravelFreeEndpointSourceMismatch");
        return {true,std::move(out),{}};
    }
    if(!step.applied_payment||drafts.size()-before.recipe_begin!=1)return fail("PrivateShopTravelPaymentRecipeRequired");
    const auto& payment=*step.applied_payment;const auto& recipe=drafts[before.recipe_begin];
    if(payment.wallet_id!=before.payer_before.id||payment.delta>0||payment.delta==std::numeric_limits<Integer>::min()||!payment.require_spendable||!same_identity(recipe.identity,before.identity)||recipe.next_instance_before!=before.next_instance_id||recipe.next_instance_after!=before.next_instance_id||recipe.recipe.operations.size()!=1||!recipe.recipe.expected_versions.empty()||recipe.operations.size()!=1)return fail("PrivateShopTravelPaymentRecipeMismatch");
    const auto* actual=std::get_if<WalletDelta>(&recipe.recipe.operations.front());
    if(!actual||actual->wallet_id!=payment.wallet_id||actual->delta!=payment.delta||actual->require_spendable!=payment.require_spendable)return fail("PrivateShopTravelPaymentOperationMismatch");
    const auto& op=recipe.operations.front();
    if(op.operation_index!=0||op.next_instance_before!=before.next_instance_id||op.next_instance_after!=before.next_instance_id||op.wallets.size()!=1||!op.inventories.empty()||!op.lot_flows.empty())return fail("PrivateShopTravelPaymentResourcesMismatch");
    const auto& change=op.wallets.front();Integer expected_balance=0;
    if(change.wallet_id!=payment.wallet_id||!change.before||!same_wallet(*change.before,before.payer_before)||!same_wallet(change.after,*wallet)||__builtin_add_overflow(before.payer_before.balance,payment.delta,&expected_balance)||expected_balance<0)return fail("PrivateShopTravelPaymentWalletMismatch");
    auto expected=before.payer_before;expected.balance=expected_balance;
    if(!same_wallet(expected,change.after))return fail("PrivateShopTravelPaymentWalletMismatch");
    out.paid_amount=-payment.delta;out.actual_payment=change;return {true,std::move(out),{}};
}
}
