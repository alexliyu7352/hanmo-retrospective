// 复用原单recipe投影，只拼同捕获真实相邻批次值；不另写资源账或重新执行原FIFO。
#include "private_shop_freight_capture_projection.hpp"
#include "resident/storage/common_store.hpp"
#include "resident/storage/selected_resource_execution.hpp"
#include <map>
#include <set>
namespace hanmo::resident {
struct PrivateShopFreightProjectionBoundary::State {
 const CommonStore* original_common=nullptr;
 std::shared_ptr<const ResourceExecutionCaptureIdentity> capture_identity;
 WorldIdentity identity;Integer next_instance=0;std::size_t recipe_begin=0;
 std::vector<PrivateShopFreightCargoSelection> selection;
 std::vector<PrivateShopFreightLotFrame> original_lots;
 std::map<StableId,std::vector<StableId>> inventory_lot_order;
};
// 仅桥接此叶的opaque值，不给调用者修改已取前界的出口。
struct PrivateShopFreightCaptureProjectionAccess {static const auto& state(const PrivateShopFreightProjectionBoundary& b){return b.state_;}};
namespace {
using Key=std::pair<StableId,StableId>;
// 容器原批序也影响FIFO；不是相同ID集合即可替换。
std::vector<StableId> lot_order(const InventoryView& inventory){std::vector<StableId> ids;for(const auto& lot:inventory.lots)ids.push_back(lot.id);return ids;}
bool same_owner(const Owner&a,const Owner&b){return owner_equal(a,b)&&a.extra==b.extra&&a.field_order==b.field_order;}
bool same_frame(const PrivateShopFreightLotFrame&a,const PrivateShopFreightLotFrame&b){const auto& x=a.lot;const auto& y=b.lot;return a.inventory_id==b.inventory_id&&same_owner(a.inventory_owner,b.inventory_owner)&&a.capacity==b.capacity&&a.revision==b.revision&&x.id==y.id&&x.item_id==y.item_id&&x.quantity==y.quantity&&same_owner(x.owner,y.owner)&&x.reserved_by==y.reserved_by&&same_resource_lot_value(x,y);}
// 当前只作同候选端点核对，不能拿这个查找结果补历史缺行。
Outcome<PrivateShopFreightLotFrame> actual_lot(const CommonStore& c,const StableId& inventory,const StableId& id){const auto* source=c.inventory(inventory);const LotView* found=nullptr;if(source)for(const auto& lot:source->lots)if(lot.id==id){if(found||lot.item_id.empty()||lot.quantity<=0)return {false,{},common_failure("PrivateShopFreightCaptureLotAmbiguous",id).error};found=&lot;}if(!source||source->id!=inventory||!found)return {false,{},common_failure("PrivateShopFreightCaptureLotRequired",id).error};return {true,{source->id,source->owner,source->capacity,source->revision,*found},{}};}
}
Outcome<PrivateShopFreightProjectionBoundary> capture_private_shop_freight_projection_boundary(const CommonStore& c,const std::vector<PrivateShopFreightCargoSelection>& selected){
 auto token=c.resource_execution_capture_identity();if(!token||selected.empty())return {false,{},common_failure("PrivateShopFreightCaptureBoundaryRequired").error};auto state=std::make_shared<PrivateShopFreightProjectionBoundary::State>();state->original_common=&c;state->capture_identity=std::move(token);state->identity=c.state().identity;state->next_instance=c.state().next_instance_id;state->recipe_begin=c.resource_execution_drafts().size();state->selection=selected;std::set<StableId> inventories,ids;
 for(const auto& scope:selected){if(scope.inventory_id.empty()||scope.lot_ids.empty()||!inventories.insert(scope.inventory_id).second)return {false,{},common_failure("PrivateShopFreightCargoSelectionInvalid",scope.inventory_id).error};for(const auto& id:scope.lot_ids){if(id.empty()||!ids.insert(id).second)return {false,{},common_failure("PrivateShopFreightCargoSelectionInvalid",id).error};auto lot=actual_lot(c,scope.inventory_id,id);if(!lot.ok)return {false,{},lot.error};state->original_lots.push_back(std::move(lot.value));}state->inventory_lot_order.emplace(scope.inventory_id,lot_order(*c.inventory(scope.inventory_id)));}
 PrivateShopFreightProjectionBoundary out;out.state_=std::move(state);return {true,std::move(out),{}};
}
Outcome<PrivateShopFreightCaptureProjection> read_private_shop_freight_capture_projection(const CommonStore& c,const PrivateShopFreightProjectionBoundary& boundary){
 const auto fail=[](const StableId& code,const StableId& id=StableId{})->Outcome<PrivateShopFreightCaptureProjection>{return {false,{},common_failure(code,id).error};};const auto& b=PrivateShopFreightCaptureProjectionAccess::state(boundary);const auto& drafts=c.resource_execution_drafts();const auto& current=c.state().identity;
 if(!b||b->original_common!=&c||!b->capture_identity||b->capture_identity!=c.resource_execution_capture_identity()||b->identity.world_id!=current.world_id||b->identity.execution_epoch!=current.execution_epoch||b->identity.tick>current.tick||b->identity.revision>current.revision||b->next_instance>c.state().next_instance_id||b->recipe_begin>drafts.size())return fail("PrivateShopFreightCaptureScopeMismatch");
 PrivateShopFreightCaptureProjection out{current,b->recipe_begin,drafts.size(),{}, {}};auto selected=b->selection;auto orders=b->inventory_lot_order;std::map<Key,PrivateShopFreightLotFrame> known;std::set<StableId> seen;
 for(const auto& lot:b->original_lots){known.emplace(Key{lot.inventory_id,lot.lot.id},lot);seen.insert(lot.lot.id);}Integer prior_id=b->next_instance,prior_tick=b->identity.tick,prior_revision=b->identity.revision;
 for(std::size_t i=b->recipe_begin;i<drafts.size();++i){const auto& recipe=drafts[i];if(recipe.identity.world_id!=b->identity.world_id||recipe.identity.execution_epoch!=b->identity.execution_epoch||recipe.identity.tick<prior_tick||recipe.identity.tick>current.tick||recipe.identity.revision<prior_revision||recipe.identity.revision>current.revision||recipe.next_instance_before<prior_id||recipe.next_instance_after<recipe.next_instance_before||recipe.next_instance_after>c.state().next_instance_id)return fail("PrivateShopFreightCaptureRecipeOrder");auto projected=project_private_shop_freight_recipe(recipe,selected);if(!projected.ok)return {false,{},projected.error};
  for(const auto& op:recipe.operations){
   for(const auto& frame:op.inventories){auto order=orders.find(frame.inventory_id);if(order!=orders.end()){if(!frame.before||order->second!=lot_order(*frame.before))return fail("PrivateShopFreightCaptureLotOrderMismatch",frame.inventory_id);order->second=lot_order(frame.after);}}
   auto projected_row=std::find_if(projected.value.operations.begin(),projected.value.operations.end(),[&](const auto& row){return row.operation_index==op.operation_index;});if(projected_row==projected.value.operations.end())continue;const auto& row=*projected_row;
   for(const auto& after:row.after_lots)if(!orders.count(after.inventory_id)){auto frame=std::find_if(op.inventories.begin(),op.inventories.end(),[&](const auto& v){return v.inventory_id==after.inventory_id;});if(frame==op.inventories.end())return fail("PrivateShopFreightCaptureContinuityMismatch",after.lot.id);orders.emplace(after.inventory_id,lot_order(frame->after));}
   for(const auto& before:row.before_lots){auto prior=known.find({before.inventory_id,before.lot.id});if(prior==known.end()||!same_frame(prior->second,before))return fail("PrivateShopFreightCaptureContinuityMismatch",before.lot.id);}
   // 单recipe读会认实际新分号；这里另保跨recipe已耗尽ID不能被后续假映射重新使用。
   for(const auto& flow:row.original_flows)if(flow.kind==ResourceLotFlowDraft::Kind::transfer||flow.kind==ResourceLotFlowDraft::Kind::reserve_split||flow.kind==ResourceLotFlowDraft::Kind::selected_reserve_split||flow.kind==ResourceLotFlowDraft::Kind::selected_transfer_split){if(!seen.insert(flow.destination_lot_id).second)return fail("PrivateShopFreightCaptureLotReused",flow.destination_lot_id);}
   for(const auto& before:row.before_lots)known.erase({before.inventory_id,before.lot.id});for(const auto& after:row.after_lots){known[{after.inventory_id,after.lot.id}]=after;seen.insert(after.lot.id);}
  }
  selected=projected.value.remaining_lots;out.recipes.push_back(std::move(projected.value));prior_id=recipe.next_instance_after;prior_tick=recipe.identity.tick;prior_revision=recipe.identity.revision;
 }
 for(const auto& row:orders){const auto* actual=c.inventory(row.first);if(!actual||row.second!=lot_order(*actual))return fail("PrivateShopFreightCaptureLotOrderMismatch",row.first);}
 std::size_t count=0;for(const auto& scope:selected)for(const auto& id:scope.lot_ids){auto original=known.find({scope.inventory_id,id});if(original==known.end())return fail("PrivateShopFreightCaptureContinuityMismatch",id);auto actual=actual_lot(c,scope.inventory_id,id);if(!actual.ok)return {false,{},actual.error};if(!same_frame(original->second,actual.value))return fail("PrivateShopFreightCaptureEndpointMismatch",id);out.remaining_lots.push_back(std::move(actual.value));++count;}if(count!=known.size())return fail("PrivateShopFreightCaptureContinuityMismatch");return {true,std::move(out),{}};
}
}
