#include "population_selection_source.hpp"
namespace hanmo::resident {
Outcome<const PopulationSelection*> read_population_selection_source(const LocalEconomyState& economy,const PopulationTransportState& state,const SourceRef& ref,Integer schema){
 if(schema!=population_selection_source_schema||ref.domain!="local_economy"||ref.kind!=population_selection_source_kind||ref.id.empty())return {false,{},common_failure("UnsupportedPopulationSelectionSource",ref.id,ErrorKind::unsupported_capability).error};
 const auto* row=state.selections.find(ref.id);const auto* current=row?state.source_revisions.find(row->source.cohort_id):nullptr;
 if(!row||row->id!=ref.id||!current||!economy.cohorts.find(row->source.cohort_id)||row->source.population_revision<1||row->source.population_revision>current->revision||row->selected_persons<1||row->selected_labor_eligible<0||row->selected_labor_eligible>row->selected_persons||row->created_tick<0||!valid_source_ref(row->owner_business_ref)||!valid_source_ref(row->release_gate))return {false,{},common_failure("PopulationSelectionSourceMissing",ref.id).error};
 auto original=read_population_revision_source(economy,state,row->source);if(!original.ok)return {false,{},original.error};if(row->selected_persons>original.value.headcount||row->selected_labor_eligible>original.value.labor_capacity||original.value.labor_capacity-row->selected_labor_eligible>original.value.headcount-row->selected_persons)return {false,{},common_failure("PopulationSelectionSourceQuantityMismatch",ref.id).error};
 const auto occurrences=std::count(current->active_selection_refs.begin(),current->active_selection_refs.end(),row->id);const auto* materialized=state.materializations.find(row->id);
 if((row->state==PopulationSelectionPhase::reserved?occurrences!=1:occurrences!=0)||(row->state==PopulationSelectionPhase::materialized)!=bool(materialized))return {false,{},common_failure("PopulationSelectionSourceLifecycleMismatch",ref.id).error};
 if(materialized&&(materialized->selection_id!=row->id||materialized->source_cohort_ref.cohort_id!=row->source.cohort_id||materialized->transfer_ref!=row->result_transfer_ref||!materialized->result_cohort_ref.transport_binding_id))return {false,{},common_failure("PopulationSelectionSourceMaterializationMismatch",ref.id).error};
 if(row->purpose==PopulationSelectionPurpose::household_purchase&&(row->selected_persons!=1||row->selected_labor_eligible!=1||row->partition_policy!=PopulationPartitionPolicy::labor_eligible_only))return {false,{},common_failure("HouseholdBuyerSelectionScope",ref.id).error};
 return {true,row,{}};
}
Outcome<SourceRef> population_selection_source_ref(const LocalEconomyState& economy,const PopulationTransportState& state,const StableId& id){SourceRef ref{"local_economy",population_selection_source_kind,id};auto row=read_population_selection_source(economy,state,ref);if(!row.ok)return {false,{},row.error};return {true,std::move(ref),{}};}
}
