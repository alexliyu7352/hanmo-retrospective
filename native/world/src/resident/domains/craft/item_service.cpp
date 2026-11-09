#include "resident/domains/commerce/repair_service/repair_service.hpp"
// 实际producer提供已应用输出；S9冻结品质/绝对期限，Common原recipe唯一增减实物。
#include "item_service.hpp"
#include "item_condition_contract.hpp"
#include "repair_contract.hpp"
#include "resident/kernel/read_stamp.hpp"
#include <set>
#include <map>
#include <tuple>
namespace hanmo::resident { namespace {
Status bad(const StableId& code,const StableId& id={}){return common_failure(code,id);}
bool same_key(const ItemOutputKey&a,const ItemOutputKey&b){return same_source_ref(a.source_ref,b.source_ref)&&a.output_ordinal==b.output_ordinal&&a.source_schema==b.source_schema;}
const ItemOutputAdoption* adoption(const std::vector<ItemOutputAdoption>& rows,const ItemOutputKey& key){for(const auto& row:rows)if(same_key(row.key,key))return &row;return nullptr;}
bool sorted_unique(const std::vector<StableId>& rows){return std::all_of(rows.begin(),rows.end(),[](const StableId& x){return !x.empty();})&&std::adjacent_find(rows.begin(),rows.end(),[](const StableId&a,const StableId&b){return a>=b;})==rows.end();}
Status source_valid(const ItemOutputSourceView& v,const ItemServiceCatalog& c){
 if(!valid_source_ref(v.key.source_ref)||v.key.output_ordinal<0||v.key.source_schema<1||v.definition_id.empty()||v.quantity<1||v.produced_tick<0||!sorted_unique(v.fit_tag_ids)||(v.quality_score&&(*v.quality_score<0||*v.quality_score>100)))return bad("InvalidItemOutputSource",v.definition_id);
 if(v.unique_instance){if(v.quantity!=1||!v.quality_score||!v.maker_ref||v.max_durability<=0||v.repair_policy_id.empty()||v.expires_at_tick||!v.property_policy_id.empty()||!v.fit_tag_ids.empty())return bad("InvalidUniqueItemOutput",v.definition_id);return common_success();}
 const auto* p=c.property_policies.find(v.property_policy_id);if(!p||p->definition_id!=v.definition_id||p->version!=v.property_policy_version||!sorted_unique(p->allowed_fit_tag_ids))return bad("ItemPropertyPolicyMismatch",v.property_policy_id);
 for(const auto& tag:v.fit_tag_ids)if(!std::binary_search(p->allowed_fit_tag_ids.begin(),p->allowed_fit_tag_ids.end(),tag))return bad("ItemPropertyTagNotAllowed",tag);
 if(p->nonperishable){if(p->shelf_life_ticks||v.expires_at_tick)return bad("ItemExpiryPolicyMismatch",p->id);}
 else{if(!p->shelf_life_ticks||*p->shelf_life_ticks<=0||v.produced_tick>INT64_MAX-*p->shelf_life_ticks||!v.expires_at_tick||*v.expires_at_tick!=v.produced_tick+*p->shelf_life_ticks)return bad("ItemExpiryPolicyMismatch",p->id);}
 return common_success();
}
Outcome<Integer> current_item_tick(const CommonRead&common,const CurrentMomentRead*clock){if(!clock)return {true,common.tick(),{}};auto at=clock->read_current_moment();if(!at.ok)return {false,{},at.error};auto valid=compare_world_moments(at.value,at.value);if(!valid.ok)return {false,{},valid.error};if(at.value.half_day_tick<common.tick())return {false,{},bad("ItemSourceClockBeforeCommon").error};return {true,at.value.half_day_tick,{}};}
Status property_output_value(const LotProperties&p,const ItemOutputAdoption&row,const ItemOutputSourceView&v){
 if(p.definition_id!=v.definition_id||p.schema_version!=1||p.quality_score!=v.quality_score||p.fit_tag_ids!=v.fit_tag_ids||p.produced_tick!=v.produced_tick||p.expires_at_tick!=v.expires_at_tick||!same_source_ref(p.origin_receipt_ref,row.key.source_ref)||p.maker_ref.has_value()!=v.maker_ref.has_value()||(p.maker_ref&&(p.maker_ref->kind!=v.maker_ref->kind||p.maker_ref->id!=v.maker_ref->id))||p.property_policy_id!=v.property_policy_id||p.property_policy_version!=v.property_policy_version)return bad("LotPropertiesSourceMismatch",p.properties_id);
 return common_success();
}
Status instance_output_value(const ItemInstance&i,const ItemOutputAdoption&row,const ItemOutputSourceView&v){
 if(i.item_definition_id!=v.definition_id||i.creation_fact_id!=row.key.source_ref.id||!v.maker_ref||i.maker_ref.kind!=v.maker_ref->kind||i.maker_ref.id!=v.maker_ref->id||!v.quality_score||i.quality_score!=*v.quality_score||i.repair_policy_id!=v.repair_policy_id)return bad("ItemCreationMismatch",i.id);
 return common_success();
}
Status termination_source_value(const ItemInstancesState&instances,const ItemOutputSourceRead&sources,const ItemTerminationResult&t,Integer current_tick){
 if(t.id.empty()||!valid_source_ref(t.cause_ref)||t.cause_schema<1||t.usage_ordinal<0||t.source_inventory_id.empty()||t.source_lot_id.empty()||t.instance_version_before<1||t.instance_version_before==INT64_MAX||t.instance_version_after!=t.instance_version_before+1||t.effective_tick<0||t.effective_tick>current_tick)return bad("InvalidItemTermination",t.id);
 const auto*i=instances.instances.find(t.instance_id);if(!i||i->id!=t.instance_id||i->state!=ItemInstanceState::destroyed||i->instance_version!=t.instance_version_after)return bad("ItemTerminationStateMismatch",t.id);
 const auto source=sources.read_item_termination(t.cause_ref,t.cause_schema,t.usage_ordinal,t.instance_id);if(!source.ok)return {false,{},source.error};
 if(!same_source_ref(source.value.source_ref,t.cause_ref)||source.value.source_schema!=t.cause_schema||source.value.instance_id!=t.instance_id||source.value.usage_ordinal!=t.usage_ordinal||source.value.effective_tick!=t.effective_tick)return bad("ItemTerminationSourceMismatch",t.id);
 return common_success();
}
struct SemanticBinding {CommonStore& common;const SelectedLotSemanticRead* previous;SemanticBinding(CommonStore& c,const SelectedLotSemanticRead& r):common(c),previous(c.selected_lot_semantics()){common.bind_selected_lot_semantics(&r);}~SemanticBinding(){common.bind_selected_lot_semantics(previous);}};
}
Outcome<const ItemOutputAdoption*> read_item_instance_original_output(const CommonStore&common,const ItemInstancesState&instances,const ItemServiceCatalog&catalog,const ItemOutputSourceRead&sources,const StableId&id,const CurrentMomentRead*clock){
 auto fail=[](const Status&s){return Outcome<const ItemOutputAdoption*>{false,nullptr,s.error};};
 const auto*item=instances.instances.find(id);
 if(id.empty()||!item||item->id!=id)return fail(bad("ItemInstanceSourceRequired",id));
 const ItemOutputAdoption*row=nullptr;
 for(const auto&candidate:instances.output_adoptions)if(candidate.instance_id&&*candidate.instance_id==id){if(row)return fail(bad("DuplicateItemOutputBinding",id));row=&candidate;}
 if(!row||row->properties_ref||row->original_lot_id.empty()||row->original_quantity!=1)return fail(bad("InvalidItemOutputAdoption",id));
 for(const auto&other:instances.output_adoptions)if(&other!=row&&((same_source_ref(other.key.source_ref,row->key.source_ref)&&other.key.output_ordinal==row->key.output_ordinal)||other.original_lot_id==row->original_lot_id))return fail(bad("InvalidItemOutputAdoption",id));
 const auto output=sources.read_item_output(row->key,row->source_kind);if(!output.ok)return {false,nullptr,output.error};
 auto checked=source_valid(output.value,catalog);if(!checked.ok)return fail(checked);
 const auto tick=current_item_tick(common,clock);if(!tick.ok)return {false,nullptr,tick.error};
 const auto&v=output.value;
 if(!same_key(v.key,row->key)||v.source_kind!=row->source_kind||!v.unique_instance||v.definition_id!=row->definition_id||v.quantity!=row->original_quantity||v.produced_tick>tick.value)return fail(bad("ItemOutputAdoptionMismatch",row->key.source_ref.id));
 checked=instance_output_value(*item,*row,v);if(!checked.ok)return fail(checked);
 return {true,row,{}};
}
Status visit_item_instance_original_output_pins(const CommonStore&common,const ItemInstancesState&instances,const ItemServiceCatalog&catalog,const ItemOutputSourceRead&sources,const StableId&id,ItemOutputSourcePinSink&sink,const CurrentMomentRead*clock){
 const auto row=read_item_instance_original_output(common,instances,catalog,sources,id,clock);if(!row.ok)return {false,{},row.error};
 return sink.original_output(row.value->key,row.value->source_kind);
}
Outcome<const ItemTerminationResult*> read_item_original_termination(const CommonStore&common,const ItemInstancesState&instances,const ItemOutputSourceRead&sources,const StableId&id,const CurrentMomentRead*clock){
 const ItemTerminationResult*row=nullptr;
 for(const auto&candidate:instances.terminations)if(candidate.id==id){if(row)return {false,nullptr,bad("InvalidItemTermination",id).error};row=&candidate;}
 if(!row)return {false,nullptr,bad("ItemTerminationSourceRequired",id).error};
 for(const auto&other:instances.terminations)if(&other!=row&&other.instance_id==row->instance_id)return {false,nullptr,bad("InvalidItemTermination",id).error};
 const auto tick=current_item_tick(common,clock);if(!tick.ok)return {false,nullptr,tick.error};
 const auto checked=termination_source_value(instances,sources,*row,tick.value);if(!checked.ok)return {false,nullptr,checked.error};
 return {true,row,{}};
}
Outcome<const ItemOutputAdoption*> read_lot_properties_original_output(const CommonStore&common,const LotPropertiesState&properties,const ItemServiceCatalog&catalog,const ItemOutputSourceRead&sources,const LotPropertiesRef&ref,const CurrentMomentRead*clock){
 auto fail=[](const Status&s){return Outcome<const ItemOutputAdoption*>{false,nullptr,s.error};};
 const auto*p=properties.properties.find(ref.properties_id);
 if(ref.properties_id.empty()||!p||p->properties_id!=ref.properties_id)return fail(bad("LotPropertiesSourceRequired",ref.properties_id));
 const ItemOutputAdoption*row=nullptr;
 for(const auto&candidate:properties.output_adoptions)if(candidate.properties_ref&&candidate.properties_ref->properties_id==ref.properties_id){if(row)return fail(bad("DuplicateItemOutputBinding",ref.properties_id));row=&candidate;}
 if(!row||row->instance_id||row->original_lot_id.empty()||row->original_quantity<1)return fail(bad("InvalidItemOutputAdoption",ref.properties_id));
 for(const auto&other:properties.output_adoptions)if(&other!=row&&((same_source_ref(other.key.source_ref,row->key.source_ref)&&other.key.output_ordinal==row->key.output_ordinal)||other.original_lot_id==row->original_lot_id))return fail(bad("InvalidItemOutputAdoption",ref.properties_id));
 const auto output=sources.read_item_output(row->key,row->source_kind);if(!output.ok)return {false,nullptr,output.error};
 auto checked=source_valid(output.value,catalog);if(!checked.ok)return fail(checked);
 const auto tick=current_item_tick(common,clock);if(!tick.ok)return {false,nullptr,tick.error};
 const auto&v=output.value;
 if(!same_key(v.key,row->key)||v.source_kind!=row->source_kind||v.unique_instance||v.definition_id!=row->definition_id||v.quantity!=row->original_quantity||v.produced_tick>tick.value)return fail(bad("ItemOutputAdoptionMismatch",row->key.source_ref.id));
 checked=property_output_value(*p,*row,v);if(!checked.ok)return fail(checked);
 return {true,row,{}};
}
Status visit_lot_properties_original_pins(const CommonStore&common,const LotPropertiesState&properties,const ItemServiceCatalog&catalog,const ItemOutputSourceRead&sources,const LotPropertiesRef&ref,LotPropertiesSourcePinSink&sink,const CurrentMomentRead*clock){
 const auto row=read_lot_properties_original_output(common,properties,catalog,sources,ref,clock);if(!row.ok)return {false,{},row.error};
 auto status=sink.original_output(row.value->key,row.value->source_kind);if(!status.ok)return status;
 const auto&property=*properties.properties.find(ref.properties_id);
 return sink.property_policy(property.property_policy_id,property.property_policy_version);
}
ItemResourceAccess::ItemResourceAccess(const ItemInstancesState&i,const LotPropertiesState&p,const ItemServiceCatalog&c,const ItemOutputSourceRead&s,const ItemConsumptionScope*x,const CurrentMomentRead*clock):instances_(i),properties_(p),catalog_(c),sources_(s),consumption_(x),actual_clock_(clock){}
Status ItemResourceAccess::validate_live_lot(const CommonRead&,const StableId&,const LotView& lot)const{
 if(lot.extra||lot.owner.extra||(lot.item_instance_id&&lot.lot_properties_ref))return bad("LotReferenceConflict",lot.id);
 if(lot.item_instance_id){const auto* i=instances_.instances.find(*lot.item_instance_id);if(!i||i->id!=*lot.item_instance_id||i->item_definition_id!=lot.item_id||i->state!=ItemInstanceState::live||lot.quantity!=1)return bad("InvalidLiveItemInstance",lot.id);}
 if(lot.lot_properties_ref){const auto* p=properties_.properties.find(lot.lot_properties_ref->properties_id);if(!p||p->properties_id!=lot.lot_properties_ref->properties_id||p->definition_id!=lot.item_id)return bad("InvalidLotPropertiesReference",lot.id);}
 return common_success();
}
Status ItemResourceAccess::validate_selected_use(const CommonRead& common,const StableId& inventory,const LotView& lot,Integer quantity,SelectedLotUse use)const{
 auto valid=validate_live_lot(common,inventory,lot);if(!valid.ok)return valid;if(quantity<1||quantity>lot.quantity)return bad("InvalidQuantity",lot.id);
 if(lot.item_instance_id){if(quantity!=1||lot.quantity!=1)return bad("UniqueInstanceSplit",lot.id);if(use==SelectedLotUse::consume){if(!consumption_||consumption_->instance_id!=*lot.item_instance_id)return bad("UniqueItemTerminationSourceRequired",lot.id);auto source=sources_.read_item_termination(consumption_->source_ref,consumption_->source_schema,consumption_->usage_ordinal,*lot.item_instance_id);if(!source.ok)return {false,{},source.error};if(!same_source_ref(source.value.source_ref,consumption_->source_ref)||source.value.source_schema!=consumption_->source_schema||source.value.usage_ordinal!=consumption_->usage_ordinal||source.value.instance_id!=*lot.item_instance_id||source.value.effective_tick!=common.tick())return bad("ItemTerminationSourceMismatch",lot.id);}}
 if(lot.lot_properties_ref&&quantity<lot.quantity){const auto* p=properties_.properties.find(lot.lot_properties_ref->properties_id);const auto* policy=p?catalog_.property_policies.find(p->property_policy_id):nullptr;if(!policy||!policy->splittable)return bad("LotPropertySplitForbidden",lot.id);}
 return common_success();
}
Status ItemResourceAccess::validate_materialized_lot(const CommonRead& common,const StableId& inventory,const LotView& lot)const{
 auto valid=validate_live_lot(common,inventory,lot);if(!valid.ok)return valid;auto tick=current_item_tick(common,actual_clock_);if(!tick.ok)return {false,{},tick.error};
 const auto& rows=lot.item_instance_id?instances_.output_adoptions:properties_.output_adoptions;
 const ItemOutputAdoption* found=nullptr;for(const auto& row:rows)if((lot.item_instance_id&&row.instance_id==lot.item_instance_id)||(lot.lot_properties_ref&&row.properties_ref&&row.properties_ref->properties_id==lot.lot_properties_ref->properties_id)){if(found)return bad("DuplicateItemOutputBinding",lot.id);found=&row;}
 if(!found||!found->original_lot_id.empty()||found->definition_id!=lot.item_id||found->original_quantity!=lot.quantity)return bad("ItemMaterializationSourceRequired",lot.id);
 auto source=sources_.read_item_output(found->key,found->source_kind);if(!source.ok)return {false,{},source.error};auto checked=source_valid(source.value,catalog_);if(!checked.ok)return checked;if(!same_key(source.value.key,found->key)||source.value.definition_id!=lot.item_id||source.value.quantity!=lot.quantity||source.value.produced_tick!=tick.value)return bad("ItemMaterializationSourceMismatch",lot.id);return common_success();
}
Outcome<ItemMaterialization> materialize_item_output_in_candidate(CommonStore& common,ItemInstancesState& instances,LotPropertiesState& properties,const ItemServiceCatalog& catalog,const ItemOutputSourceRead& sources,const ItemOutputKey& key,ItemOutputKind kind,const StableId& inventory,const Owner& owner,const CurrentMomentRead*clock){
 auto fail=[](const Status& s)->Outcome<ItemMaterialization>{return {false,{},s.error};};
 auto output=sources.read_item_output(key,kind);if(!output.ok)return {false,{},output.error};auto checked=source_valid(output.value,catalog);if(!checked.ok)return fail(checked);const auto& v=output.value;
 if(!same_key(v.key,key)||v.source_kind!=kind)return fail(bad("ItemOutputKeyMismatch",key.source_ref.id));
 auto& rows=v.unique_instance?instances.output_adoptions:properties.output_adoptions;
 if(const auto* old=adoption(rows,key)){if(old->source_kind!=kind||old->definition_id!=v.definition_id||old->original_quantity!=v.quantity||old->original_lot_id.empty())return fail(bad("ItemOutputReplayMismatch",key.source_ref.id));return {true,{old->instance_id,old->properties_ref,old->original_lot_id},{}};}
 auto tick=current_item_tick(common,clock);if(!tick.ok)return {false,{},tick.error};
 if(v.produced_tick!=tick.value||!common.resource_execution_capture_identity())return fail(bad("ItemOutputCandidateCaptureRequired",key.source_ref.id));
 const auto* inv=common.inventory(inventory);if(!inv||!common.valid_resource_owner(owner))return fail(bad("ItemOutputDestinationInvalid",inventory));
 auto id=common.next_id(v.unique_instance?"item_instance":"lot_properties");if(!id.ok)return {false,{},id.error};
 ItemOutputAdoption adopted{key,kind,v.definition_id,v.quantity,{},{},{}};
 if(v.unique_instance){ItemInstance row{id.value,v.definition_id,key.source_ref.id,*v.maker_ref,*v.quality_score,v.max_durability,v.max_durability,v.repair_policy_id,ItemInstanceState::live,1,{}};if(!instances.instances.insert(id.value,std::move(row)))return fail(bad("DuplicateItemInstance",id.value));adopted.instance_id=id.value;}
 else{LotProperties row{id.value,v.definition_id,1,v.quality_score,v.fit_tag_ids,v.produced_tick,v.expires_at_tick,key.source_ref,v.maker_ref,v.property_policy_id,v.property_policy_version};if(!properties.properties.insert(id.value,std::move(row)))return fail(bad("DuplicateLotProperties",id.value));adopted.properties_ref=LotPropertiesRef{id.value};}
 rows.push_back(adopted);ItemResourceAccess access(instances,properties,catalog,sources,nullptr,clock);SemanticBinding binding(common,access);
 const auto draft_index=common.resource_execution_drafts().size();ResourceRecipe recipe;recipe.expected_versions.push_back(common.read_version({ResourceKey::Kind::inventory,inventory}));recipe.operations.push_back(AddLot{inventory,v.definition_id,v.quantity,owner,{},adopted.instance_id,adopted.properties_ref});checked=common.apply_resources(recipe);if(!checked.ok)return fail(checked);
 const auto& drafts=common.resource_execution_drafts();if(drafts.size()!=draft_index+1||drafts.back().operations.size()!=1||drafts.back().operations[0].lot_flows.size()!=1)return fail(bad("ItemOutputCaptureMismatch",key.source_ref.id));const auto& flow=drafts.back().operations[0].lot_flows[0];if(flow.kind!=ResourceLotFlowDraft::Kind::add||flow.destination_inventory_id!=inventory||flow.item_id!=v.definition_id||flow.quantity!=v.quantity)return fail(bad("ItemOutputCaptureMismatch",key.source_ref.id));
 rows.back().original_lot_id=flow.destination_lot_id;return {true,{adopted.instance_id,adopted.properties_ref,flow.destination_lot_id},{}};
}
Status check_item_material_use(const ItemInstancesState& instances,const LotPropertiesState& properties,const ItemServiceCatalog& catalog,const LotView& lot,const ItemMaterialUse& rule,Integer at){
 if(at<0||lot.item_id!=rule.definition_id||lot.quantity<=0)return bad("MaterialDefinitionMismatch",lot.id);std::optional<Integer> quality;
 if(lot.item_instance_id){const auto* i=instances.instances.find(*lot.item_instance_id);if(!i||i->state!=ItemInstanceState::live||i->item_definition_id!=lot.item_id||lot.quantity!=1)return bad("InvalidLiveItemInstance",lot.id);quality=i->quality_score;if(rule.require_properties||!rule.required_fit_tag_ids.empty())return bad("MaterialPropertiesRequired",lot.id);}
 else if(lot.lot_properties_ref){const auto* p=properties.properties.find(lot.lot_properties_ref->properties_id);if(!p||p->definition_id!=lot.item_id||!catalog.property_policies.find(p->property_policy_id)||p->produced_tick>at)return bad("InvalidLotPropertiesReference",lot.id);if(p->expires_at_tick&&at>=*p->expires_at_tick&&!rule.allow_expired)return bad("MaterialExpired",lot.id);quality=p->quality_score;for(const auto& tag:rule.required_fit_tag_ids)if(!std::binary_search(p->fit_tag_ids.begin(),p->fit_tag_ids.end(),tag))return bad("MaterialFitMissing",tag);}
 else if(rule.require_properties||rule.minimum_quality||!rule.required_fit_tag_ids.empty())return bad("MaterialPropertiesRequired",lot.id);
 if(rule.minimum_quality&&(!quality||*quality<*rule.minimum_quality))return bad("MaterialQualityInsufficient",lot.id);return common_success();
}
Outcome<ItemTerminationResult> consume_unique_item_in_candidate(CommonStore& common,ItemInstancesState& instances,const LotPropertiesState& properties,const ItemServiceCatalog& catalog,const ItemOutputSourceRead& sources,const ItemConsumptionScope& scope,const CargoSelection& selection,const Owner& owner,const ReservationRef& reservation){
 for(const auto& result:instances.terminations)if(same_source_ref(result.cause_ref,scope.source_ref)&&result.cause_schema==scope.source_schema&&result.usage_ordinal==scope.usage_ordinal&&result.instance_id==scope.instance_id)return {true,result,{}};
 auto* instance=instances.instances.find(scope.instance_id);if(!instance||instance->state!=ItemInstanceState::live||instance->instance_version==INT64_MAX)return {false,{},bad("InvalidItemTermination",scope.instance_id).error};const auto* inventory=common.inventory(selection.inventory_id);const LotView* lot=nullptr;if(inventory)for(const auto& row:inventory->lots)if(row.id==selection.lot_id)lot=&row;if(!lot||lot->item_instance_id!=std::optional<StableId>{scope.instance_id}||selection.quantity!=1)return {false,{},bad("ItemTerminationLotMismatch",selection.lot_id).error};
 ItemResourceAccess access(instances,properties,catalog,sources,&scope);SemanticBinding binding(common,access);auto checked=access.validate_selected_use(common,selection.inventory_id,*lot,1,SelectedLotUse::consume);if(!checked.ok)return {false,{},checked.error};auto id=common.next_id("item_termination");if(!id.ok)return {false,{},id.error};const auto before=instance->instance_version;
 ResourceRecipe recipe;recipe.expected_versions.push_back(common.read_version({ResourceKey::Kind::inventory,selection.inventory_id}));recipe.operations.push_back(ConsumeSelectedLots{selection.inventory_id,{selection},{owner},reservation});checked=common.apply_resources(recipe);if(!checked.ok)return {false,{},checked.error};instance->state=ItemInstanceState::destroyed;++instance->instance_version;ItemTerminationResult result{id.value,instance->id,scope.source_ref,scope.source_schema,scope.usage_ordinal,common.tick(),before,instance->instance_version,selection.inventory_id,selection.lot_id};instances.terminations.push_back(result);return {true,std::move(result),{}};
}
Status validate_item_service(const CommonStore& common,const ItemInstancesState& instances,const LotPropertiesState& properties,const ItemServiceCatalog& catalog,const ItemOutputSourceRead& sources,Integer selected_version,const ItemConditionOriginalRead* conditions,const CurrentMomentRead* clock,const RepairReadContext* repair){
 if(selected_version!=1&&selected_version!=2&&selected_version!=3)return bad("UnsupportedItemInstancesVersion");
 if(selected_version!=3&&(!instances.repair_jobs.acceptances.rows().empty()||!instances.repair_jobs.unstarted_interruptions.rows().empty()||!instances.repair_jobs.jobs.rows().empty()||!instances.repair_jobs.controls.rows().empty()||!instances.repair_jobs.completions.rows().empty()||!instances.repair_jobs.interruptions.rows().empty()))return bad("UnselectedRepairJobs");
 if(selected_version!=3&&!repair_service_contracts_empty(instances.repair_service_contracts))return bad("UnselectedRepairServiceContracts");
 if(selected_version==1&&!instances.condition_results.empty())return bad("UnselectedItemConditionResults");
 Integer current_tick=common.tick();
 if(selected_version==2||selected_version==3){if(!conditions||!clock)return bad("ItemConditionOriginalReadRequired");auto actual=clock->read_current_moment();if(!actual.ok)return {false,{},actual.error};auto valid=compare_world_moments(actual.value,actual.value);if(!valid.ok)return {false,{},valid.error};if(actual.value.half_day_tick<common.tick())return bad("ItemConditionClockBeforeCommon");current_tick=actual.value.half_day_tick;}

 std::set<std::tuple<StableId,StableId,StableId,Integer>> output_keys;std::set<StableId> instance_bindings,property_bindings,original_lots,termination_ids,terminated_instances;std::map<StableId,Integer> live_counts;
 ItemResourceAccess access(instances,properties,catalog,sources);
 for(const auto& row:common.state().inventories.rows())for(const auto& lot:row.second.lots){auto checked=access.validate_live_lot(common,row.first,lot);if(!checked.ok)return checked;if(lot.item_instance_id)++live_counts[*lot.item_instance_id];}
 auto validate_outputs=[&](const std::vector<ItemOutputAdoption>& rows,bool unique)->Status{
  for(const auto& row:rows){const auto key=std::make_tuple(row.key.source_ref.domain,row.key.source_ref.kind,row.key.source_ref.id,row.key.output_ordinal);if(!output_keys.insert(key).second||row.original_lot_id.empty()||!original_lots.insert(row.original_lot_id).second||row.original_quantity<1||row.instance_id.has_value()!=unique||row.properties_ref.has_value()==unique)return bad("InvalidItemOutputAdoption",row.key.source_ref.id);
   auto output=sources.read_item_output(row.key,row.source_kind);if(!output.ok)return {false,{},output.error};auto checked=source_valid(output.value,catalog);if(!checked.ok)return checked;const auto& v=output.value;if(!same_key(v.key,row.key)||v.source_kind!=row.source_kind||v.unique_instance!=unique||v.definition_id!=row.definition_id||v.quantity!=row.original_quantity||v.produced_tick>current_tick)return bad("ItemOutputAdoptionMismatch",row.key.source_ref.id);
   if(unique){const auto* i=instances.instances.find(*row.instance_id);if(!i||!instance_bindings.insert(i->id).second)return bad("ItemCreationMismatch",*row.instance_id);auto instance=instance_output_value(*i,row,v);if(!instance.ok)return instance;}
   else{const auto* p=properties.properties.find(row.properties_ref->properties_id);if(!p||!property_bindings.insert(p->properties_id).second)return bad("LotPropertiesSourceMismatch",row.properties_ref->properties_id);auto property=property_output_value(*p,row,v);if(!property.ok)return property;}
  }return common_success();
 };
 auto checked=validate_outputs(instances.output_adoptions,true);if(!checked.ok)return checked;checked=validate_outputs(properties.output_adoptions,false);if(!checked.ok)return checked;
 if(instance_bindings.size()!=instances.instances.size()||property_bindings.size()!=properties.properties.size())return bad("ItemSourceRegistryMismatch");
 for(const auto& t:instances.terminations){if(t.id.empty()||!termination_ids.insert(t.id).second||!terminated_instances.insert(t.instance_id).second)return bad("InvalidItemTermination",t.id);auto source=termination_source_value(instances,sources,t,current_tick);if(!source.ok)return source;}
 for(const auto& row:instances.instances.rows()){const auto& i=row.second;if(row.first!=i.id||i.id.empty()||i.quality_score<0||i.quality_score>100||i.max_durability<=0||i.durability<0||i.durability>i.max_durability||i.instance_version<1||(i.state!=ItemInstanceState::live&&i.state!=ItemInstanceState::destroyed))return bad("InvalidItemInstance",row.first);if(i.state==ItemInstanceState::live){if(live_counts[i.id]!=1||terminated_instances.count(i.id))return bad("UniqueInstanceLotMismatch",i.id);}else if(live_counts[i.id]||!terminated_instances.count(i.id))return bad("DestroyedInstanceHasLot",i.id);}
 for(const auto& row:properties.properties.rows())if(row.first!=row.second.properties_id)return bad("LotPropertiesKeyMismatch",row.first);
 if(selected_version==2||selected_version==3){auto history=validate_item_condition_history({common,instances,sources,*conditions,*clock,selected_version});if(!history.ok)return history;}
 if(selected_version==3){if(!repair||&repair->common!=&common||&repair->items!=&instances||&repair->properties!=&properties||&repair->item_catalog!=&catalog||&repair->conditions.outputs!=&sources||&repair->conditions.original!=conditions||&repair->clock!=clock)return bad("RepairOriginalReadRequired");auto jobs=validate_repair_jobs(*repair);if(!jobs.ok)return jobs;if(!repair->commercial_sources||&repair->commercial_sources->common!=&common||&repair->commercial_sources->items!=&instances||repair->commercial_sources->repair!=repair)return common_failure("RepairOriginalCommercialReaderRequired",{},ErrorKind::unsupported_capability);return validate_repair_service_contracts(*repair->commercial_sources);}
 return common_success();
}
}
