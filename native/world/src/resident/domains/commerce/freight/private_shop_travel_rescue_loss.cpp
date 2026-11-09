// 原救助逐批Adjust与真实返起点配对，只交同候选货损来源，不产生运费或库存历史。
#include "private_shop_travel_rescue_loss.hpp"
#include "resident/storage/common_store.hpp"
#include "resident/domains/travel/travel.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include <set>
namespace hanmo::resident {namespace {
// 保留完整兼容字段、预约与顺序，不拿数量相等代替同一批。
bool same_owner(const Owner&a,const Owner&b){return owner_equal(a,b)&&a.extra==b.extra&&a.field_order==b.field_order;}
bool same_identity(const WorldIdentity&a,const WorldIdentity&b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
bool same_lot(const LotView&a,const LotView&b){return a.id==b.id&&a.item_id==b.item_id&&a.quantity==b.quantity&&same_owner(a.owner,b.owner)&&a.reserved_by==b.reserved_by&&a.extra==b.extra&&a.field_order==b.field_order;}
bool same_inventory(const InventoryView&a,const InventoryView&b){if(a.id!=b.id||!same_owner(a.owner,b.owner)||a.capacity!=b.capacity||a.revision!=b.revision||a.field_order!=b.field_order||a.lots.size()!=b.lots.size())return false;for(std::size_t i=0;i<a.lots.size();++i)if(!same_lot(a.lots[i],b.lots[i]))return false;return true;}
bool same_action(const ActionCoreView&a,const ActionCoreView&b){return a.id==b.id&&a.kind==b.kind&&a.actor_id==b.actor_id&&a.participants==b.participants&&a.start_tick==b.start_tick&&a.remaining_ticks==b.remaining_ticks;}
// 原Location各variant逐值核对，不以resolved位置替代被实际移动的原位置。
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
bool same_adjustment(const AdjustLots&a,const AdjustLots&b){if(a.inventory_id!=b.inventory_id||a.bump_revision!=b.bump_revision||a.lots.size()!=b.lots.size())return false;for(std::size_t i=0;i<a.lots.size();++i)if(a.lots[i].lot_id!=b.lots[i].lot_id||a.lots[i].delta!=b.lots[i].delta)return false;return true;}
}
Outcome<PrivateShopTravelRescueLossBoundary> capture_private_shop_travel_rescue_loss_boundary(const CommonStore& c,const TravelState& t,const TravelCatalog& catalog,const StableId& id){
 const auto fail=[&](const StableId& code)->Outcome<PrivateShopTravelRescueLossBoundary>{return {false,{},common_failure(code,id).error};};
 const auto token=c.resource_execution_capture_identity();const auto* action=c.action(id);const auto* rescue=t.rescues.find(id);const auto* actor=action?c.actor(action->actor_id):nullptr;const auto* bag=actor?c.inventory(actor->inventory_id):nullptr;const auto* origin=rescue?catalog.settlements.find(rescue->origin):nullptr;const auto serial=organization_instance_sequence(id,"action");
 if(!token||!serial||*serial>=c.state().next_instance_id||!action||action->id!=id||action->kind!="rescue"||action->remaining_ticks!=0||action->start_tick<0||action->start_tick>c.tick()||!rescue||t.actions.find(id)||t.road_actions.find(id)||!actor||actor->action_id!=id||!bag||!owner_equal(bag->owner,{Owner::Kind::actor,actor->id,{}})||!origin||origin->id!=rescue->origin)return fail("PrivateShopTravelRescueBoundaryRequired");
 std::set<StableId> lots;for(const auto& lot:bag->lots)if(lot.id.empty()||lot.item_id.empty()||lot.quantity<=0||!lots.insert(lot.id).second)return fail("PrivateShopTravelRescueInventoryInvalid");
 return {true,{&c,&t,&catalog,token,c.state().identity,*action,*rescue,origin->entry_place,actor->location,*bag,c.state().next_instance_id,c.resource_execution_drafts().size()},{}};
}
Outcome<PrivateShopTravelRescueLoss> read_private_shop_travel_rescue_loss(const CommonStore& c,const TravelState& t,const TravelCatalog& catalog,const PrivateShopTravelRescueLossBoundary& b,const TravelRescueCompletionDraft& done){
 const auto fail=[&](const StableId& code)->Outcome<PrivateShopTravelRescueLoss>{return {false,{},common_failure(code,b.action.id).error};};
 const auto* action=c.action(b.action.id);const auto* actor=c.actor(b.action.actor_id);const auto* rescue=t.rescues.find(b.action.id);const auto* origin=catalog.settlements.find(b.rescue.origin);const auto* inventory=c.inventory(b.before_inventory.id);const auto& drafts=c.resource_execution_drafts();
 if(b.original_common!=&c||b.original_travel!=&t||b.original_catalog!=&catalog||!b.capture_identity||b.capture_identity!=c.resource_execution_capture_identity()||!same_identity(b.identity,c.state().identity)||b.next_instance_before!=c.state().next_instance_id||!action||!same_action(*action,b.action)||b.action.kind!="rescue"||b.action.remaining_ticks!=0||!actor||actor->action_id!=b.action.id||actor->inventory_id!=b.before_inventory.id||!rescue||rescue->origin!=b.rescue.origin||t.actions.find(b.action.id)||t.road_actions.find(b.action.id)||!origin||origin->id!=b.rescue.origin||origin->entry_place!=b.origin_entry_place||!inventory||b.recipe_begin>drafts.size()||drafts.size()-b.recipe_begin!=1)return fail("PrivateShopTravelRescueScopeMismatch");
 const Location returned=SettlementLocation{b.rescue.origin,b.origin_entry_place};
 if(!same_action(done.action,b.action)||done.source_tick!=b.identity.tick||done.rescue.origin!=b.rescue.origin||!same_location(done.before_location,b.before_location)||!same_location(done.after_location,returned)||!same_location(actor->location,returned)||!same_inventory(done.before_inventory,b.before_inventory)||!same_inventory(done.after_inventory,*inventory))return fail("PrivateShopTravelRescueCompletionMismatch");
 const auto& recipe=drafts[b.recipe_begin];if(!same_identity(recipe.identity,b.identity)||recipe.next_instance_before!=b.next_instance_before||recipe.next_instance_after!=b.next_instance_before||recipe.recipe.operations.size()!=1||recipe.operations.size()!=1||!recipe.recipe.expected_versions.empty())return fail("PrivateShopTravelRescueRecipeMismatch");
 const auto* adjust=std::get_if<AdjustLots>(&recipe.recipe.operations.front());const auto& op=recipe.operations.front();
 if(!adjust||adjust->inventory_id!=b.before_inventory.id||!adjust->bump_revision||op.operation_index!=0||op.next_instance_before!=b.next_instance_before||op.next_instance_after!=b.next_instance_before||!op.wallets.empty()||op.inventories.size()!=1)return fail("PrivateShopTravelRescueRecipeMismatch");
 const auto& frame=op.inventories.front();if(frame.inventory_id!=b.before_inventory.id||!frame.before||!same_inventory(*frame.before,b.before_inventory)||!same_inventory(frame.after,*inventory))return fail("PrivateShopTravelRescueInventoryMismatch");
 // 严格复算原writer每个本人批次的整数损耗，包含零腿；外人货和所有预约保持原值。
 AdjustLots expected;expected.inventory_id=b.before_inventory.id;auto after=b.before_inventory;std::vector<PrivateShopTravelRescueLotLoss> losses;
 for(auto& lot:after.lots)if(owner_equal(lot.owner,{Owner::Kind::actor,b.action.actor_id,{}})){const Integer lost=lot.quantity/10;expected.lots.push_back({lot.id,-lost});losses.push_back({lot.id,lot.item_id,lot.quantity,lost,lot.quantity-lost});lot.quantity-=lost;}
 if(__builtin_add_overflow(after.revision,Integer{1},&after.revision)||!same_adjustment(*adjust,expected)||!same_adjustment(done.applied_adjustment,expected)||!same_inventory(after,*inventory)||op.lot_flows.size()!=losses.size())return fail("PrivateShopTravelRescueLossMismatch");
 for(std::size_t i=0;i<losses.size();++i){const auto& loss=losses[i];const auto& flow=op.lot_flows[i];if(flow.kind!=ResourceLotFlowDraft::Kind::adjust||flow.source_inventory_id!=b.before_inventory.id||flow.destination_inventory_id!=b.before_inventory.id||flow.source_lot_id!=loss.lot_id||flow.destination_lot_id!=loss.lot_id||flow.item_id!=loss.item_id||flow.quantity!=-loss.lost_quantity)return fail("PrivateShopTravelRescueLotMismatch");}
 return {true,{b.identity,done,recipe,std::move(losses)},{}};
}
}
