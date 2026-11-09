#include "craft_materials.hpp"
#include "resident/storage/selected_resource_execution.hpp"
#include <limits>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char*c,const StableId&id={}){throw Failure{common_failure(c,id).error};}
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
Integer add(Integer a,Integer b){if(a<0||b<0||a>std::numeric_limits<Integer>::max()-b)fail("CraftMaterialQuantityOverflow");return a+b;}
bool owner(const Owner&a,const Owner&b){return !a.extra&&!b.extra&&a.kind==b.kind&&a.id==b.id;}
bool properties(const std::optional<LotPropertiesRef>&a,const std::optional<LotPropertiesRef>&b){return a.has_value()==b.has_value()&&(!a||a->properties_id==b->properties_id);}
const LotView& lot(const CommonStore&c,const CargoSelection&s){const auto*i=c.inventory(s.inventory_id);const LotView*found=nullptr;if(i)for(const auto&v:i->lots)if(v.id==s.lot_id){if(found)fail("DuplicateCraftMaterialLot",s.lot_id);found=&v;}if(!found||s.quantity<1||s.quantity>found->quantity)fail("CraftMaterialSelectionUnavailable",s.lot_id);return *found;}
const CraftJob& actual_job(const CraftMaterialReadContext&x,const StableId&id){const auto*j=x.jobs.jobs.find(id);if(!j||j->id!=id||j->terms.job_id!=id||!same_source_ref(j->terms.acceptance_ref,craft_job_source(id)))fail("CraftActualJobRequired",id);const auto*h=x.common.state().facts.find(id);if(!h||h->domain!="crafting"||h->kind!="craft_job")fail("CraftAcceptanceHeaderRequired",id);return *j;}
const CraftMaterialOption& alternative(const CraftRecipeDef&r,const CraftMaterialSelection&s){if(s.material_index<0||static_cast<std::uint64_t>(s.material_index)>=r.materials.size())fail("CraftMaterialIndexInvalid",r.recipe_id);const auto&m=r.materials[static_cast<std::size_t>(s.material_index)];if(s.alternative_index<0||static_cast<std::uint64_t>(s.alternative_index)>=m.alternatives.size())fail("CraftMaterialAlternativeInvalid",r.recipe_id);return m.alternatives[static_cast<std::size_t>(s.alternative_index)];}
void use(const CraftMaterialReadContext&x,const CraftMaterialOption&a,const LotView&v,const WorldMoment&at){if(v.item_instance_id)fail("CraftConsumableMaterialRequired",v.id);take(check_item_material_use(x.instances,x.properties,x.items,v,{a.definition_id,a.minimum_quality,a.required_fit_tag_ids,a.require_properties,false},at.half_day_tick));}
const ResourceOperationExecutionDraft& applied(CommonStore&c,const ResourceOperation&op){if(!c.resource_execution_capture_identity())fail("CraftResourceCaptureRequired");const auto before=c.resource_execution_drafts().size();ResourceRecipe recipe;recipe.operations.push_back(op);std::visit([&](const auto&v){using T=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<T,ReserveSelectedLots>||std::is_same_v<T,ConsumeSelectedLots>)recipe.expected_versions.push_back(c.read_version({ResourceKey::Kind::inventory,v.inventory_id}));},op);take(c.apply_resources(recipe));const auto&drafts=c.resource_execution_drafts();if(drafts.size()!=before+1||drafts.back().operations.size()!=1)fail("CraftActualMaterialExecutionRequired");take(validate_selected_resource_execution(op,drafts.back().operations.front()));return drafts.back().operations.front();}
}
SourceRef craft_job_source(const StableId&id){return {"crafting","craft_job",id};}
SourceRef craft_stage_source(const StableId&id){return {"crafting","craft_stage_result",id};}
SourceRef craft_gate_source(const StableId&id){return {"crafting","craft_work_gate",id};}
SourceRef craft_quality_source(const StableId&id){return {"crafting","craft_quality_result",id};}
SourceRef craft_completion_source(const StableId&id){return {"crafting","craft_completed",id};}
SourceRef craft_pace_change_source(const StableId&id){return {"crafting","craft_pace_changed",id};}
SourceRef craft_control_source(const StableId&id){return {"crafting","craft_control",id};}
ReservationRef craft_job_reservation(const StableId&id){return {ReservationRef::Kind::domain_activity,id,"crafting","craft_job"};}
Status validate_craft_material_selection(const CraftMaterialReadContext&x,const CraftTerms&t,const WorldMoment&at){try{
 take(compare_world_moments(at,at));const auto*r=take(read_craft_recipe(x.recipes,t.recipe_ref));if(t.material_selections.empty())fail("CraftMaterialsRequired",t.job_id);
 std::vector<Integer> indices(r->materials.size(),-1),quantities(r->materials.size(),0);
 for(std::size_t i=0;i<t.material_selections.size();++i){const auto&s=t.material_selections[i];const auto&a=alternative(*r,s);const auto&v=lot(x.common,s.selected);if(!owner(v.owner,s.legal_owner)||!x.common.valid_resource_owner(s.legal_owner)||v.extra)fail("CraftMaterialOwnerMismatch",s.selected.lot_id);
  if(!v.reserved_by.empty()&&(!t.order_ref||v.reserved_by!=t.material_holder))fail("CraftMaterialAlreadyReserved",v.id);
  for(std::size_t j=0;j<i;++j)if(s.selected.inventory_id==t.material_selections[j].selected.inventory_id&&s.selected.lot_id==t.material_selections[j].selected.lot_id)fail("DuplicateCraftMaterialSelection",v.id);
  use(x,a,v,at);auto index=static_cast<std::size_t>(s.material_index);if(indices[index]!=-1&&indices[index]!=s.alternative_index)fail("CraftMaterialMixedAlternativeRequired",t.job_id);indices[index]=s.alternative_index;quantities[index]=add(quantities[index],s.selected.quantity);
 }
 for(std::size_t i=0;i<quantities.size();++i)if(quantities[i]!=r->materials[i].quantity)fail("CraftMaterialRequirementMismatch",t.job_id);
 if(t.quality_inputs.material_fit!=take(read_craft_material_fit(*r,indices)))fail("CraftMaterialFitMismatch",t.job_id);
 return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Outcome<std::vector<CraftReservedMaterial>> reserve_craft_materials_in_candidate(CommonStore&c,const CraftMaterialReadContext&x,const StableId&id){try{
 if(&c!=&x.common)fail("CraftCandidateContextMismatch",id);const auto&j=actual_job(x,id);const auto&t=j.terms;if(!j.original_reservations.empty())return {true,j.original_reservations,{}};if(j.phase!=CraftJobPhase::reserved||!j.stage_result_refs.empty())fail("CraftReservationPhaseMismatch",id);
 take(validate_craft_material_selection(x,t,t.accepted_at));if(t.material_holder.empty()||(!t.order_ref&&t.material_holder!=craft_job_reservation(id)))fail("CraftOriginalMaterialHolderRequired",id);
 std::vector<CraftReservedMaterial> out;
 for(std::size_t index=0;index<t.material_selections.size();++index){const auto&s=t.material_selections[index];const auto&before=lot(c,s.selected);const auto item=before.item_id;const auto original_owner=before.owner;const auto original_properties=before.lot_properties_ref;
  if(before.reserved_by==t.material_holder){if(before.quantity!=s.selected.quantity)fail("CraftExternalMaterialPartitionSourceRequired",id);out.push_back({static_cast<Integer>(index),s.selected,s.selected,item,original_owner,original_properties});continue;}
  const ResourceOperation op=ReserveSelectedLots{s.selected.inventory_id,{s.selected},{s.legal_owner},t.material_holder};const auto&draft=applied(c,op);if(draft.lot_flows.size()!=1)fail("CraftReservationFlowMismatch",id);const auto&flow=draft.lot_flows.front();
  if(flow.source_lot_id!=s.selected.lot_id||flow.source_inventory_id!=s.selected.inventory_id||flow.destination_inventory_id!=s.selected.inventory_id||flow.item_id!=item||flow.quantity!=s.selected.quantity||(flow.kind!=ResourceLotFlowDraft::Kind::selected_reserve_whole&&flow.kind!=ResourceLotFlowDraft::Kind::selected_reserve_split))fail("CraftReservationFlowMismatch",id);
  out.push_back({static_cast<Integer>(index),s.selected,{s.selected.inventory_id,flow.destination_lot_id,flow.quantity},item,original_owner,original_properties});
 }
 return {true,std::move(out),{}};
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Outcome<Integer> read_craft_consumed_selection_quantity(const CraftMaterialReadContext&x,const CraftJob&j,Integer selection){try{Integer sum=0;for(const auto&id:j.stage_result_refs){const auto*s=x.jobs.stages.find(id);if(!s||s->job_id!=j.id)fail("CraftStageHistoryRequired",id);for(const auto&c:s->consumed)if(c.selection_ordinal==selection)sum=add(sum,c.actual.quantity);}return {true,sum,{}};}catch(Failure&e){return {false,{},std::move(e.error)};}}
Outcome<std::vector<CraftConsumedMaterial>> consume_craft_stage_materials_in_candidate(CommonStore&c,const CraftMaterialReadContext&x,const StableId&id,Integer ordinal,const WorldMoment&at){try{
 if(&c!=&x.common)fail("CraftCandidateContextMismatch",id);const auto&j=actual_job(x,id);const auto*r=take(read_craft_recipe(x.recipes,j.terms.recipe_ref));if(ordinal<0||static_cast<std::uint64_t>(ordinal)>=r->stages.size()||j.current_ordinal!=ordinal||j.phase!=CraftJobPhase::working)fail("CraftStageMutationOrderMismatch",id);
 const auto&spec=r->stages[static_cast<std::size_t>(ordinal)];std::vector<CraftConsumedMaterial> out;
 for(const auto&input:spec.inputs){Integer remaining=input.quantity;for(const auto&held:j.original_reservations){if(remaining==0)break;if(held.selection_ordinal<0||static_cast<std::uint64_t>(held.selection_ordinal)>=j.terms.material_selections.size())fail("CraftMaterialReservationIndexMismatch",id);const auto&selected=j.terms.material_selections[static_cast<std::size_t>(held.selection_ordinal)];if(selected.material_index!=input.material_index)continue;
   const auto consumed=take(read_craft_consumed_selection_quantity(x,j,held.selection_ordinal));if(consumed>held.held.quantity)fail("CraftMaterialConsumptionOverflow",id);const Integer quantity=std::min(remaining,held.held.quantity-consumed);if(!quantity)continue;CargoSelection actual{held.held.inventory_id,held.held.lot_id,quantity};const auto&v=lot(c,actual);
   if(v.reserved_by!=j.terms.material_holder||v.item_id!=held.definition_id||!owner(v.owner,held.original_owner)||!properties(v.lot_properties_ref,held.properties_ref))fail("CraftReservedMaterialChanged",v.id);use(x,alternative(*r,selected),v,at);
   const ResourceOperation op=ConsumeSelectedLots{actual.inventory_id,{actual},{held.original_owner},j.terms.material_holder};const auto&draft=applied(c,op);if(draft.lot_flows.size()!=1)fail("CraftConsumptionFlowMismatch",id);const auto&flow=draft.lot_flows.front();if(flow.kind!=ResourceLotFlowDraft::Kind::selected_consume||flow.source_inventory_id!=actual.inventory_id||flow.source_lot_id!=actual.lot_id||flow.item_id!=held.definition_id||flow.quantity!=quantity||!flow.destination_lot_id.empty())fail("CraftConsumptionFlowMismatch",id);
   out.push_back({held.selection_ordinal,input.material_index,actual,held.definition_id,held.original_owner,held.properties_ref});remaining-=quantity;
  }if(remaining)fail("CraftReservedMaterialInsufficient",id);
 }
 return {true,std::move(out),{}};
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Status validate_craft_material_history(const CraftMaterialReadContext&x,const CraftJob&j){try{
 actual_job(x,j.id);const auto*r=take(read_craft_recipe(x.recipes,j.terms.recipe_ref));if(j.original_reservations.size()!=j.terms.material_selections.size()||j.current_ordinal<0||static_cast<std::uint64_t>(j.current_ordinal)!=j.stage_result_refs.size())fail("CraftMaterialHistoryCoverageMismatch",j.id);
 for(std::size_t i=0;i<j.original_reservations.size();++i){const auto&held=j.original_reservations[i];const auto&s=j.terms.material_selections[i];const auto&a=alternative(*r,s);if(held.selection_ordinal!=static_cast<Integer>(i)||held.original.inventory_id!=s.selected.inventory_id||held.original.lot_id!=s.selected.lot_id||held.original.quantity!=s.selected.quantity||held.held.inventory_id!=s.selected.inventory_id||held.held.lot_id.empty()||held.held.quantity!=s.selected.quantity||held.definition_id!=a.definition_id||!owner(held.original_owner,s.legal_owner))fail("CraftOriginalReservationMismatch",j.id);
  const auto consumed=take(read_craft_consumed_selection_quantity(x,j,static_cast<Integer>(i)));if(consumed>held.held.quantity)fail("CraftConsumedMaterialOverflow",j.id);
  if(j.phase==CraftJobPhase::reserved||j.phase==CraftJobPhase::working||j.phase==CraftJobPhase::paused||j.phase==CraftJobPhase::awaiting_output_space){const auto remaining=held.held.quantity-consumed;if(remaining){const auto&v=lot(x.common,{held.held.inventory_id,held.held.lot_id,remaining});if(v.quantity!=remaining||v.reserved_by!=j.terms.material_holder||v.item_id!=held.definition_id||!owner(v.owner,held.original_owner)||!properties(v.lot_properties_ref,held.properties_ref))fail("CraftRemainingReservationMismatch",j.id);}}
 }
 for(std::size_t ordinal=0;ordinal<j.stage_result_refs.size();++ordinal){const auto*s=x.jobs.stages.find(j.stage_result_refs[ordinal]);if(!s||s->id!=j.stage_result_refs[ordinal]||s->job_id!=j.id||s->ordinal!=static_cast<Integer>(ordinal)||ordinal>=r->stages.size())fail("CraftStageSourceOrderMismatch",j.id);const auto*h=x.common.state().facts.find(s->id);if(!h||h->domain!="crafting"||h->kind!="craft_stage_result")fail("CraftStageHeaderRequired",s->id);
  const auto&spec=r->stages[ordinal];for(const auto&input:spec.inputs){Integer total=0;for(const auto&actual:s->consumed)if(actual.material_index==input.material_index)total=add(total,actual.actual.quantity);if(total!=input.quantity)fail("CraftStageConsumptionMismatch",s->id);}
  for(const auto&actual:s->consumed){if(actual.selection_ordinal<0||static_cast<std::uint64_t>(actual.selection_ordinal)>=j.original_reservations.size())fail("CraftConsumptionSelectionMismatch",s->id);const auto&held=j.original_reservations[static_cast<std::size_t>(actual.selection_ordinal)];const auto&selected=j.terms.material_selections[static_cast<std::size_t>(actual.selection_ordinal)];bool expected=false;for(const auto&input:spec.inputs)if(input.material_index==actual.material_index)expected=true;if(!expected||selected.material_index!=actual.material_index||actual.actual.inventory_id!=held.held.inventory_id||actual.actual.lot_id!=held.held.lot_id||actual.actual.quantity<1||actual.definition_id!=held.definition_id||!owner(actual.original_owner,held.original_owner)||!properties(actual.properties_ref,held.properties_ref))fail("CraftConsumptionOriginalMappingMismatch",s->id);}
 }
 return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}
}
