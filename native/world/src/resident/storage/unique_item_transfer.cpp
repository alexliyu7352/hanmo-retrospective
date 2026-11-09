#include "unique_item_transfer.hpp"
#include "selected_resource_execution.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <type_traits>
#include <utility>
#include <limits>
// Existing diagnostic identifiers are retained with the promoted algorithm.
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={},ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,id,kind).error};}
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{std::move(value.error)};return std::move(value.value);}
CheckpointCopyLease reserve_effect(const OriginalUniqueItemTransferEffectBuildView&v){return CheckpointCopyLease::reserve();}
bool owner(const Owner& a,const Owner& b){return !a.extra&&!b.extra&&a.kind==b.kind&&a.id==b.id;}

const LotView* find(const InventoryView& inventory,const StableId& id){
 const LotView* value=nullptr;for(const auto& lot:inventory.lots)if(lot.id==id){if(value)fail("RepairTransferDuplicateLot",id);value=&lot;}return value;
}
bool inventory_header(const InventoryView& a,const InventoryView& b,Integer increment){
 return a.id==b.id&&owner_equal(a.owner,b.owner)&&a.owner.extra==b.owner.extra&&a.owner.field_order==b.owner.field_order&&a.capacity==b.capacity&&a.field_order==b.field_order&&a.revision>=0&&a.revision<=std::numeric_limits<Integer>::max()-increment&&b.revision==a.revision+increment;
}
void current_image(const CommonStore& c,const InventoryView& actual){
 const auto* current=c.inventory(actual.id);if(!current||!inventory_header(actual,*current,0)||current->lots.size()!=actual.lots.size())fail("RepairTransferCurrentImageMismatch",actual.id);
 for(std::size_t n=0;n<actual.lots.size();++n)if(!same_resource_lot_value(actual.lots[n],current->lots[n]))fail("RepairTransferCurrentImageMismatch",actual.id);
}

}
static_assert(std::is_nothrow_move_constructible_v<OriginalUniqueItemTransferEffect>);
static_assert(std::is_nothrow_swappable_v<OriginalUniqueItemTransferEffect>);
static_assert(std::is_nothrow_move_constructible_v<CheckpointCopyLease>);
static_assert(std::is_nothrow_swappable_v<CheckpointCopyLease>);
OwnedUniqueItemTransferEffect::OwnedUniqueItemTransferEffect(const OriginalUniqueItemTransferEffectBuildView&v):memory_(reserve_effect(v)),value_{StableId(v.source_inventory_id),StableId(v.destination_inventory_id),StableId(v.lot_id),StableId(v.instance_id),StableId(v.item_definition_id),v.legal_owner,v.source_reservation,v.source_inventory_revision_before,v.destination_inventory_revision_before}{}
OwnedUniqueItemTransferEffect::OwnedUniqueItemTransferEffect(OwnedUniqueItemTransferEffect&&v)noexcept:memory_(std::move(v.memory_)),value_(std::move(v.value_)){}
OwnedUniqueItemTransferEffect&OwnedUniqueItemTransferEffect::operator=(OwnedUniqueItemTransferEffect&&v)noexcept{if(this!=&v){OwnedUniqueItemTransferEffect fresh(std::move(v));swap(fresh);}return *this;}
void OwnedUniqueItemTransferEffect::swap(OwnedUniqueItemTransferEffect&v)noexcept{if(this==&v)return;using std::swap;swap(value_,v.value_);swap(memory_,v.memory_);}
Outcome<AppliedUniqueItemTransfer> apply_original_unique_item_transfer_in_candidate(CommonStore& c,const CargoSelection& selected,const StableId& destination,const StableId& instance,const Owner& legal_owner,const ReservationRef& reservation){try{
 if(selected.inventory_id.empty()||selected.lot_id.empty()||selected.quantity!=1||destination.empty()||destination==selected.inventory_id||instance.empty()||legal_owner.extra||!c.valid_resource_owner(legal_owner))fail("RepairActualUniqueItemTransferRequired",instance);
 const auto capture=c.resource_execution_capture_identity();if(!capture)fail("RepairItemResourceCaptureRequired",instance,ErrorKind::unsupported_capability);
 const auto* source=c.inventory(selected.inventory_id);const auto* target=c.inventory(destination);const auto* item=source?find(*source,selected.lot_id):nullptr;
 if(!source||!target||!item||item->quantity!=1||item->item_instance_id!=std::optional<StableId>{instance}||item->lot_properties_ref||item->extra||!owner(item->owner,legal_owner)||item->reserved_by!=reservation||find(*target,selected.lot_id))fail("RepairActualSelectedItemMismatch",instance);
 const auto count=c.resource_execution_drafts().size();ResourceRecipe request;request.expected_versions={c.read_version({ResourceKey::Kind::inventory,selected.inventory_id}),c.read_version({ResourceKey::Kind::inventory,destination})};request.operations={TransferSelectedLots{selected.inventory_id,destination,{selected},{legal_owner},reservation,std::nullopt}};
 take(c.apply_resources(request));const auto& drafts=c.resource_execution_drafts();if(c.resource_execution_capture_identity()!=capture||drafts.size()!=count+1)fail("RepairTransferActualCaptureMismatch",instance);
 const auto& recipe=drafts.back();if(recipe.operations.size()!=1||recipe.recipe.operations.size()!=1||recipe.next_instance_before!=recipe.next_instance_after)fail("RepairTransferOriginalRecipeMismatch",instance);
 const auto* operation=std::get_if<TransferSelectedLots>(&request.operations.front());const auto&applied=recipe.operations.front();
 if(!operation||applied.operation_index||applied.next_instance_before!=recipe.next_instance_before||applied.next_instance_after!=recipe.next_instance_after)fail("RepairTransferOriginalOperationMismatch",instance);
 const auto proof=take(read_selected_unique_item_transfer(*operation,applied));const auto*from=proof.source;const auto*to=proof.destination;const auto*original=proof.original;const auto*delivered=proof.delivered;const auto&flow=*proof.flow;
 if(original->item_instance_id!=std::optional<StableId>{instance}||!owner(original->owner,legal_owner)||operation->new_owner)fail("RepairTransferOriginalItemScopeMismatch",instance);
 current_image(c,from->after);current_image(c,to->after);return {true,{&recipe,&applied,&flow,original,delivered},{}};
}catch(Failure& e){return {false,{},std::move(e.error)};}}
Status validate_original_unique_item_transfer_effect_shape(const OriginalUniqueItemTransferEffect&v){
 if(v.source_inventory_id.empty()||v.destination_inventory_id.empty()||v.source_inventory_id==v.destination_inventory_id||v.lot_id.empty()||v.instance_id.empty()||v.item_definition_id.empty()||v.legal_owner.extra||v.legal_owner.id.empty()||v.source_inventory_revision_before<0||v.destination_inventory_revision_before<0||v.source_inventory_revision_before==std::numeric_limits<Integer>::max()||v.destination_inventory_revision_before==std::numeric_limits<Integer>::max())return common_failure("InvalidOriginalUniqueItemTransferEffect",v.instance_id,ErrorKind::internal);
 return common_success();
}
Outcome<OwnedUniqueItemTransferEffect> capture_original_unique_item_transfer_effect(const CommonStore&c,const AppliedUniqueItemTransfer&v){try{
 if(!c.resource_execution_capture_identity()||!v.recipe||!v.operation||!v.flow||!v.original_item||!v.delivered_item)fail("ActualUniqueItemTransferCaptureRequired");
 bool found=false;for(const auto&r:c.resource_execution_drafts())if(&r==v.recipe)found=true;
 if(!found||v.recipe->operations.size()!=1||v.recipe->recipe.operations.size()!=1||&v.recipe->operations.front()!=v.operation)fail("ForeignUniqueItemTransferCapture");
 const auto*operation=std::get_if<TransferSelectedLots>(&v.recipe->recipe.operations.front());if(!operation||operation->new_owner)fail("OriginalCustodyTransferRequired");
 const auto p=take(read_selected_unique_item_transfer(*operation,*v.operation));
 if(p.flow!=v.flow||p.original!=v.original_item||p.delivered!=v.delivered_item||!p.original->item_instance_id)fail("UniqueItemTransferCaptureIdentityMismatch");
 const OriginalUniqueItemTransferEffectBuildView fields{p.source->before->id,p.destination->before->id,p.original->id,*p.original->item_instance_id,p.original->item_id,p.original->owner,p.original->reserved_by,p.source->before->revision,p.destination->before->revision};
 OwnedUniqueItemTransferEffect out(fields);
 take(validate_original_unique_item_transfer_effect_shape(out.value()));return {true,std::move(out),{}};
}catch(Failure&e){return {false,{},std::move(e.error)};}}
}
