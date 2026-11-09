#include "repair_item_command.hpp"
#include "repair_item_command_memory.hpp"
#include "repair_service_physical_internal.hpp"
#include "repair_service_item_transfer.hpp"
#include "resident/domains/craft/craft_facility_occupancy.hpp"
#include "resident/kernel/world_copy_plan.hpp"
namespace hanmo::resident {using namespace repair_physical_detail;
namespace {
CheckpointCopyLease reserve_proposal(const RepairItemProposalBuildView&v){return CheckpointCopyLease::reserve();}
struct ActualProposal {
 CheckpointCopyLease memory; // first: retained across every fallible field move
 bool returning=false;StableId actor,job_id;MomentOwnerSource contract_ref,contact,destination_source;
 OwnedUniqueItemTransferEffect effect;ItemConditionHead condition;WorldMoment at;
 ActualProposal(const RepairItemProposalBuildView&v,OwnedUniqueItemTransferEffect&&e):memory(reserve_proposal(v)),returning(v.returning),actor(v.actor),job_id(v.job_id),contract_ref(v.contract_ref),contact(v.contact),destination_source(v.destination_source),effect(std::move(e)),condition(v.condition),at(v.at){}
 ActualProposal(const ActualProposal&)=delete;ActualProposal&operator=(const ActualProposal&)=delete;
 ActualProposal(ActualProposal&&v):memory(v.memory),returning(v.returning),actor(std::move(v.actor)),job_id(std::move(v.job_id)),contract_ref(std::move(v.contract_ref)),contact(std::move(v.contact)),destination_source(std::move(v.destination_source)),effect(std::move(v.effect)),condition(std::move(v.condition)),at(std::move(v.at)){v.memory={};}
 ActualProposal&operator=(ActualProposal&&)=delete;
};
void bound(const RepairItemCommandContext&x){
 independent(x.physical);const auto&c=x.physical.commercial;
 if(compare(take(x.stamps.read_current_moment()),now(c)))fail("RepairItemCommandClockMismatch");
 if(x.repair_write&&(&x.repair_write->read!=c.repair||&x.repair_write->stamp_authority!=&x.stamp_authority||&x.repair_write->versions!=&x.versions||!x.elapsed||&x.repair_write->read.elapsed!=x.elapsed))fail("RepairItemCommandWriteContextMismatch");
}
bool same_effect(const OriginalUniqueItemTransferEffect&a,const OriginalUniqueItemTransferEffect&b){return a.source_inventory_id==b.source_inventory_id&&a.destination_inventory_id==b.destination_inventory_id&&a.lot_id==b.lot_id&&a.instance_id==b.instance_id&&a.item_definition_id==b.item_definition_id&&owner(a.legal_owner,b.legal_owner)&&a.legal_owner.field_order==b.legal_owner.field_order&&a.source_reservation==b.source_reservation&&a.source_inventory_revision_before==b.source_inventory_revision_before&&a.destination_inventory_revision_before==b.destination_inventory_revision_before;}
ActualProposal proposal(const RepairItemCommandContext&x,const StableId&actor,const MomentOwnerSource&original,const MomentOwnerSource&contact_source,bool returning,const CargoSelection*delivery,const StableId*destination){
 bound(x);const auto&c=x.physical.commercial;const auto&t=contract(c,original);const auto&a=accepted_scope(c,t);const auto at=now(c);
 for(const auto&r:c.items.repair_service_contracts.returns.rows())if(same(r.second.contract_ref,original))fail("RepairItemAlreadyReturned",t.id,ErrorKind::rule);
 const CargoSelection*selected_input=nullptr;const StableId*target_input=nullptr;std::string_view job_id;
 if(!returning){
  if(!delivery)fail("RepairActualDeliverySelectionRequired");
  for(const auto&r:c.items.repair_service_contracts.custody_receipts.rows())if(same(r.second.contract_ref,original))fail("RepairItemAlreadyDelivered",t.id,ErrorKind::rule);
  if(job(c,t))fail("RepairDeliveryAfterStart",t.id,ErrorKind::rule);
  for(const auto&r:c.items.repair_jobs.unstarted_interruptions.rows())if(same(r.second.accepted_service_ref,{repair_accepted_source(a.id),1}))fail("RepairDeliveryAfterUnstartedTerminal",t.id,ErrorKind::rule);
  selected_input=delivery;target_input=&t.terms.custody_terms.inventory_id;
 }else{
  if(!destination||destination->empty())fail("RepairActualReturnDestinationRequired");
  const auto end=terminal(c,t);if(end.job){job_id=end.job->id;if(!x.repair_write||!x.elapsed)fail("RepairReturnS9WriteContextRequired",t.id,ErrorKind::unsupported_capability);
   if(!c.repair->occupancy||end.job->facility_occupancy_event_refs.empty())fail("RepairReturnOriginalClearanceRequired",t.id,ErrorKind::unsupported_capability);
   const auto last=take(read_craft_facility_occupancy(*c.repair->occupancy,craft_facility_occupancy_source(end.job->facility_occupancy_event_refs.back()),1));
   if(!last.original_event||last.original_event->occupancy_kind!=CraftFacilityOccupancyKind::cleared)fail("RepairReturnActualClearanceRequired",t.id,ErrorKind::unsupported_capability);
  }
  const RepairCustodyReceivedSource*held=nullptr;for(const auto&r:c.items.repair_service_contracts.custody_receipts.rows())if(same(r.second.contract_ref,original)){if(held)fail("RepairAmbiguousOriginalCustody",t.id);held=take(read_repair_custody_received(c,{repair_custody_received_source(r.first),1}));}
  if(!held)fail("RepairActualCustodyBeforeReturnRequired",t.id);selected_input=&held->actual_item_selection;target_input=destination;
 }
 const auto&selected=*selected_input;const auto&target=*target_input;
 repair_physical_detail::contact(x.physical,t,actor,contact_source,at,nullptr,returning);
 const auto current=take(read_current_face_to_face_contact_source(x.physical.agreements.common,x.physical.agreements.interactions,x.physical.agreements.interaction_catalog,x.interaction_access,actor,t.terms.provider.id,at));if(contact_source.source_schema!=1||!same_source_ref(current,contact_source.source_ref))fail("RepairCurrentHandoverContactChanged",t.id,ErrorKind::rule);
 const auto*from=c.common.inventory(selected.inventory_id);const auto*to=c.common.inventory(target);const auto&item=repair_detail::lot(c.common,selected);
 if(!from||!to||selected.quantity!=1||item.quantity!=1||item.item_instance_id!=std::optional<StableId>{t.terms.instance_id}||item.lot_properties_ref||item.extra||!owner(item.owner,t.terms.customer))fail("RepairActualSelectedItemChanged",t.id,ErrorKind::rule);
 for(const auto&lot:to->lots)if(lot.id==selected.lot_id)fail("RepairTargetAlreadyContainsOriginalItem",t.id);
 OwnedUniqueItemTransferEffect effect(OriginalUniqueItemTransferEffectBuildView{from->id,to->id,item.id,t.terms.instance_id,item.item_id,item.owner,item.reserved_by,from->revision,to->revision});
 const auto head_owner=take(read_current_item_condition_head(x.physical.agreements.conditions,t.terms.instance_id));const auto&head=head_owner.value();transfer_shape(c,t,effect.value(),{target,selected.lot_id,1},head,at,nullptr);
 auto&sources=independent(x.physical);take(sources.validate_original_repair_item_physical_scope(t,effect.value(),actor,contact_source,at,nullptr,returning));
 MomentOwnerSource actual_destination;if(returning){actual_destination=take(sources.read_original_repair_return_destination(t,target,at,nullptr));ref(actual_destination);}
 const RepairItemProposalBuildView fields{returning,actor,job_id,original,contact_source,actual_destination,head,at};
 return ActualProposal(fields,std::move(effect));
}
}
struct PreparedRepairItemCommand::Impl:CheckpointAllocatedObject {
 const RepairItemCommandContext*context=nullptr;const CommonStore*common=nullptr;const ItemInstancesState*items=nullptr;
 ActualProposal actual;std::shared_ptr<const ReadStamp>stamp;
 Impl(const RepairItemCommandContext&x,ActualProposal&&p,std::shared_ptr<const ReadStamp>s):context(&x),common(&x.physical.commercial.common),items(&x.physical.commercial.items),actual(std::move(p)),stamp(std::move(s)){}
};
struct PendingRepairItemCommand::Impl:CheckpointAllocatedObject {
 CheckpointCopyLease receipt_memory;
 std::shared_ptr<const ResourceExecutionCaptureIdentity> resource_capture;
 std::size_t resource_recipe_count=0;AppliedUniqueItemTransfer actual_transfer;
 const RepairItemCommandContext*context=nullptr;const CommonStore*common=nullptr;const ItemInstancesState*items=nullptr;WorldId world_id{};Integer execution_epoch=0;
 std::variant<RepairCustodyReceivedSource,RepairItemReturnedSource>row;RepairServiceCommandReceipt receipt;StableId started_job_id;
 explicit Impl(const RepairItemCommandContext&x):context(&x),common(&x.physical.commercial.common),items(&x.physical.commercial.items),world_id(common->state().identity.world_id),execution_epoch(common->state().identity.execution_epoch){}
};
PreparedRepairItemCommand::PreparedRepairItemCommand()=default;
PreparedRepairItemCommand::PreparedRepairItemCommand(std::unique_ptr<Impl>p):impl_(std::move(p)){}
PreparedRepairItemCommand::~PreparedRepairItemCommand()=default;
PreparedRepairItemCommand::PreparedRepairItemCommand(PreparedRepairItemCommand&&)noexcept=default;
PreparedRepairItemCommand&PreparedRepairItemCommand::operator=(PreparedRepairItemCommand&&)noexcept=default;
PendingRepairItemCommand::PendingRepairItemCommand()=default;
PendingRepairItemCommand::PendingRepairItemCommand(std::unique_ptr<Impl>p):impl_(std::move(p)){}
PendingRepairItemCommand::~PendingRepairItemCommand()=default;
PendingRepairItemCommand::PendingRepairItemCommand(PendingRepairItemCommand&&)noexcept=default;
PendingRepairItemCommand&PendingRepairItemCommand::operator=(PendingRepairItemCommand&&)noexcept=default;
struct RepairItemCommandAccess {
 static PreparedRepairItemCommand prepare(const RepairItemCommandContext&x,const StableId&actor,const RepairServiceCommandRequest&request){
  bound(x);take(validate_repair_service_contracts(x.physical.commercial));
  auto p=[&](){
   if(const auto*q=std::get_if<DeliverRepairItemRequest>(&request))return proposal(x,actor,q->contract_ref,q->actual_contact_source,false,&q->actual_item_selection,nullptr);
   if(const auto*q=std::get_if<ReceiveRepairItemRequest>(&request))return proposal(x,actor,q->contract_ref,q->actual_contact_source,true,nullptr,&q->actual_destination_inventory);
   fail("RepairPhysicalCommandRequired",actor,ErrorKind::unsupported_capability);
  }();
  auto stamp=take(x.stamps.capture({{ResourceKey::Kind::actor,actor},{ResourceKey::Kind::inventory,p.effect.value().source_inventory_id},{ResourceKey::Kind::inventory,p.effect.value().destination_inventory_id}},{{"craft","item_instance",p.effect.value().instance_id}},{p.contract_ref.source_ref.id,p.contact.source_ref.id}));
  if(!stamp||(stamp->mode()!=ReadMode::committed_current&&stamp->mode()!=ReadMode::phase_current))fail("RepairItemCommandCurrentStampRequired",actor);
  take(x.stamp_authority.validate(*stamp,x.physical.commercial.common.state().identity,stamp->mode(),x.physical.commercial.common,x.versions));
  return PreparedRepairItemCommand(std::make_unique<PreparedRepairItemCommand::Impl>(x,std::move(p),std::move(stamp)));
 }
 static PendingRepairItemCommand apply(CommonStore&c,ItemInstancesState&i,const RepairItemCommandContext&x,PreparedRepairItemCommand&&prepared){
  bound(x);auto p=std::move(prepared.impl_);if(!p||p->context!=&x||p->common!=&c||p->items!=&i||!p->stamp||compare(p->actual.at,now(x.physical.commercial)))fail("RepairPreparedItemCandidateMismatch");
  take(x.stamp_authority.validate(*p->stamp,c.state().identity,p->stamp->mode(),c,x.versions));take(validate_repair_service_contracts(x.physical.commercial));
  const auto&prior=p->actual;const CargoSelection selected{prior.effect.value().source_inventory_id,prior.effect.value().lot_id,1};
  auto actual=proposal(x,prior.actor,prior.contract_ref,prior.contact,prior.returning,prior.returning?nullptr:&selected,prior.returning?&prior.effect.value().destination_inventory_id:nullptr);
  if(!same_effect(actual.effect.value(),prior.effect.value())||!repair_detail::same_head(actual.condition,prior.condition)||actual.job_id!=prior.job_id||(prior.returning&&!same(actual.destination_source,prior.destination_source)))fail("RepairPhysicalPreparedInputsChanged",prior.effect.value().instance_id);
  auto pending=std::make_unique<PendingRepairItemCommand::Impl>(x);ReadStampWriteBoundary write(x.stamp_authority);
  const auto applied=take(apply_original_unique_item_transfer_in_candidate(c,selected,actual.effect.value().destination_inventory_id,actual.effect.value().instance_id,actual.effect.value().legal_owner,actual.effect.value().source_reservation));
  auto effect_owner=take(capture_original_unique_item_transfer_effect(c,applied));const auto&effect=effect_owner.value();if(!same_effect(effect,actual.effect.value()))fail("RepairActualTransferAdoptionMismatch",actual.effect.value().instance_id);
  pending->resource_capture=c.resource_execution_capture_identity();pending->resource_recipe_count=c.resource_execution_drafts().size();pending->actual_transfer=applied;
  const auto id=take(c.next_id(actual.returning?"repair_item_return":"repair_item_custody"));const MomentOwnerSource source{actual.returning?repair_item_returned_source(id):repair_custody_received_source(id),1};append(c,source.source_ref);
  const CargoSelection delivered{effect.destination_inventory_id,effect.lot_id,1};
  const char*command_kind=actual.returning?"receive_repair_item":"deliver_repair_item";

  pending->receipt_memory=CheckpointCopyLease::reserve();
  pending->receipt={command_kind,actual.actor,source,actual.at,{}};pending->started_job_id=StableId(actual.job_id);
  // The temporary original-effect list is charged before its vector/string construction.
  const char*effect_kind=actual.returning?"repair_return_item_transfer":"repair_custody_item_transfer";

  auto effects_memory=CheckpointCopyLease::reserve();
  std::vector<MomentOwnerSource>effects;effects.reserve(1);effects.push_back({{"commerce",effect_kind,id},1});
  const MomentOwnerSource unsealed;const LifeActorRoute pending_route;
  if(!actual.returning){
   const RepairCustodyReceivedBuildView fields{id,actual.contract_ref,actual.at,unsealed,delivered,actual.condition,effects,actual.contact,actual.actor,pending_route,effect};
   pending->row=RepairCustodyReceivedSource(fields);
  }else{
   const auto&t=contract(x.physical.commercial,actual.contract_ref);
   const RepairItemReturnedBuildView fields{id,actual.contract_ref,actual.at,unsealed,t.terms.return_terms.receiver_authority,actual.destination_source,delivered,actual.condition,effects,actual.contact,actual.actor,pending_route,effect};
   pending->row=RepairItemReturnedSource(fields);
  }
  return PendingRepairItemCommand(std::move(pending));
 }
 static const RepairServiceCommandReceipt*receipt(const PendingRepairItemCommand&p){if(!p.impl_)fail("RepairPendingItemCommandRequired");return &p.impl_->receipt;}
 // 绑定真实移交与封印；原始来源、物品或资源捕获不符时拒绝候选。
 static void bind(CommonStore &c, ItemInstancesState &i,
                 const RepairItemCommandContext &x,
                 PendingRepairItemCommand &&pending,
                 const MomentOwnerSource &seal) {
  bound(x);
  auto p = std::move(pending.impl_);
  if (!p || p->context != &x || p->common != &c || p->items != &i ||
      p->world_id != c.state().identity.world_id ||
      p->execution_epoch != c.state().identity.execution_epoch ||
      compare(p->receipt.occurred_at, now(x.physical.commercial)))
    fail("RepairPendingItemCandidateMismatch");
  // The original capture must still be the same exact last operation. Never
  // dereference borrowed draft pointers after a resource mutation/rollback.
  if (!p->resource_capture ||
      p->resource_capture != c.resource_execution_capture_identity() ||
      p->resource_recipe_count != c.resource_execution_drafts().size())
    fail("RepairPendingTransferCaptureChanged",
         p->receipt.actual_source.source_ref.id);
  const auto actual =
      take(capture_original_unique_item_transfer_effect(c, p->actual_transfer));
  std::visit(
      [&](const auto &row) {
        if (!same_effect(actual.value(), row.actual_transfer))
          fail("RepairPendingTransferEffectChanged", row.id);
      },
      p->row);
  repair_service_agreement_detail::command(
      x.physical.agreements, p->receipt.actor_id, p->receipt.command_kind,
      p->receipt.actual_source, seal, p->receipt.occurred_at);
  auto route = repair_service_agreement_detail::route(
      x.physical.agreements, p->receipt.actor_id, p->receipt.actual_source,
      seal, p->receipt.occurred_at);
  ReadStampWriteBoundary write(x.stamp_authority);
  if (auto *r = std::get_if<RepairCustodyReceivedSource>(&p->row)) {
    if (!same(p->receipt.actual_source,
              {repair_custody_received_source(r->id), 1}))
      fail("RepairPendingCustodySourceMismatch", r->id);
    const RepairCustodyReceivedBuildView fields{
        r->id,
        r->contract_ref,
        r->occurred_at,
        seal,
        r->actual_item_selection,
        r->actual_condition_head,
        r->original_resource_effect_refs,
        r->actual_contact_source,
        r->actual_actor_id,
        route,
        r->actual_transfer};
    RepairCustodyReceivedSource sealed(fields);
    r->swap(sealed);
    const auto &id = p->receipt.actual_source.source_ref.id;
    if (!i.repair_service_contracts.custody_receipts.insert(id, std::move(*r)))
      fail("DuplicateRepairCustodySource", id);
    take(read_repair_custody_received(x.physical.commercial,
                                      {repair_custody_received_source(id), 1}));
  } else {
    auto &returned = std::get<RepairItemReturnedSource>(p->row);
    if (!same(p->receipt.actual_source,
              {repair_item_returned_source(returned.id), 1}))
      fail("RepairPendingReturnSourceMismatch", returned.id);
    const RepairItemReturnedBuildView fields{
        returned.id,
        returned.contract_ref,
        returned.occurred_at,
        seal,
        returned.actual_receiver_authority,
        returned.actual_destination_source,
        returned.actual_item_selection,
        returned.actual_condition_head,
        returned.original_resource_effect_refs,
        returned.actual_contact_source,
        returned.actual_actor_id,
        route,
        returned.actual_transfer};
    RepairItemReturnedSource sealed(fields);
    returned.swap(sealed);
    const auto &id = p->receipt.actual_source.source_ref.id;
    if (!i.repair_service_contracts.returns.insert(id, std::move(returned)))
      fail("DuplicateRepairReturnSource", id);
    take(read_repair_item_returned(x.physical.commercial,
                                   {repair_item_returned_source(id), 1}));
    if (!p->started_job_id.empty()) {
      if (!x.repair_write || !x.elapsed)
        fail("RepairReturnS9WriteContextRequired", id,
             ErrorKind::unsupported_capability);
      take(adopt_repair_return_in_candidate(c, i, *x.elapsed, *x.repair_write,
                                            p->started_job_id,
                                            repair_item_returned_source(id)));
    }
  }
  take(validate_repair_service_contracts(x.physical.commercial));
}
};
Outcome<PreparedRepairItemCommand>prepare_repair_item_command(const RepairItemCommandContext&x,const StableId&a,const RepairServiceCommandRequest&r){try{return {true,RepairItemCommandAccess::prepare(x,a,r),{}};}catch(Failure&e){return {false,{},std::move(e.error)};}catch(const std::bad_alloc&){return {false,{},common_failure("RepairItemCommandMemoryExhausted",{},ErrorKind::internal).error};}}
Outcome<PendingRepairItemCommand>apply_repair_item_command_in_candidate(CommonStore&c,ItemInstancesState&i,const RepairItemCommandContext&x,PreparedRepairItemCommand&&p){try{return {true,RepairItemCommandAccess::apply(c,i,x,std::move(p)),{}};}catch(Failure&e){return {false,{},std::move(e.error)};}catch(const std::bad_alloc&){return {false,{},common_failure("RepairItemCommandMemoryExhausted",{},ErrorKind::internal).error};}}
Outcome<const RepairServiceCommandReceipt*>read_pending_repair_item_command_receipt(const PendingRepairItemCommand&p){try{return {true,RepairItemCommandAccess::receipt(p),{}};}catch(Failure&e){return {false,nullptr,std::move(e.error)};}}
Status bind_repair_item_command_in_candidate(CommonStore&c,ItemInstancesState&i,const RepairItemCommandContext&x,PendingRepairItemCommand&&p,const MomentOwnerSource&s){try{RepairItemCommandAccess::bind(c,i,x,std::move(p),s);return common_success();}catch(Failure&e){return {false,{},std::move(e.error)};}catch(const std::bad_alloc&){return common_failure("RepairItemCommandMemoryExhausted",{},ErrorKind::internal);}}
}
