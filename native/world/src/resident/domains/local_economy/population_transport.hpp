// S5唯一匿名人口选择；人数与劳力只读原PopulationCohort，运输记录只保存来源。
#pragma once
#include "local_economy_model.hpp"
#include "population_source_ref.hpp"
#include "population_labor_change.hpp"
#include "public_market_staffing_model.hpp"
#include "resident/kernel/source_ref.hpp"
namespace hanmo::resident {
class ProjectionWriter;
struct PopulationSourceRevision { StableId cohort_id;Integer revision=1;std::optional<SourceRef> condition_source_ref,location_binding_ref;std::vector<StableId> active_selection_refs; };
enum class PopulationPartitionPolicy { balanced,labor_eligible_only,nonlabor_only };
enum class PopulationSelectionPurpose { sea_transfer,custody_disposition,residence,private_enrollment,crew_service,household_purchase };
struct PopulationSelectAll {};
struct PopulationSelectCount { Integer persons=0;PopulationPartitionPolicy partition_policy=PopulationPartitionPolicy::balanced; };
using PopulationPortion=std::variant<PopulationSelectAll,PopulationSelectCount>;
struct PopulationSelectionRequest { CivilianPopulationRef source;PopulationPortion portion;PopulationSelectionPurpose purpose=PopulationSelectionPurpose::residence;SourceRef owner_business_ref,expected_location_source;std::optional<SourceRef> expected_condition_source; };
struct PopulationAvailable { Integer persons=0,labor_eligible=0; };
struct PopulationSelectionQuote { CivilianPopulationRef source;PopulationSelectionPurpose purpose;Integer persons=0,labor_eligible=0;std::optional<SourceRef> condition_ref;SourceRef location_ref;PopulationAvailable available_after_existing_selections;Integer source_revision=0; };
enum class PopulationSelectionPhase { reserved,materialized,released };
struct PopulationSelection { StableId id;SourceRef owner_business_ref;CivilianPopulationRef source;Integer selected_persons=0,selected_labor_eligible=0;PopulationPartitionPolicy partition_policy=PopulationPartitionPolicy::balanced;Integer created_tick=0;SourceRef release_gate;PopulationSelectionPhase state=PopulationSelectionPhase::reserved;std::optional<StableId> result_transfer_ref;PopulationSelectionPurpose purpose=PopulationSelectionPurpose::residence; };
struct PopulationSelectionResult { StableId selection_id;CivilianPopulationRef source_cohort_ref,result_cohort_ref;std::optional<StableId> transfer_ref;std::optional<SourceRef> condition_ref;SourceRef provision_handoff_ref;std::vector<SourceRef> obligation_continuation_refs;SourceRef location_binding_ref; };
enum class PopulationTransportPhase { active,handover_pending,released };
struct PopulationTransportBinding { StableId binding_id,transport_unit_id,transport_cohort_id;std::optional<StableId> source_population_transfer_ref;std::vector<StableId> labor_allocation_refs;SourceRef registration_source_ref;Integer created_tick=0;PopulationTransportPhase state=PopulationTransportPhase::active; };
struct PopulationRevisionSource { StableId cohort_id;Integer revision_before=0,revision_after=0;SourceRef actual_source;std::optional<SourceRef> condition_after,location_after;std::vector<PopulationLaborChange> labor_changes; };
struct PopulationTransportRelease { StableId binding_id;SourceRef actual_handover_source;Integer effective_tick=0; };
struct PopulationTransportState { OrderedTable<PopulationSourceRevision> source_revisions;OrderedTable<PopulationSelection> selections;OrderedTable<PopulationSelectionResult> materializations;OrderedTable<PopulationTransportBinding> transport_bindings;std::vector<PopulationRevisionSource> revision_sources;std::vector<PopulationTransportRelease> releases;OrderedTable<PublicMarketLaborOrigin> public_market_labor_origins;std::vector<PublicMarketLaborTransition> public_market_labor_transitions;Integer version=2; };
struct RegisterPopulationTransportFromSelection { StableId accepted_selection_id;SourceRef accepting_business_ref;Integer accepted_scope_revision=0;SourceRef transport_registration_source_ref;StableId transport_unit_id;SourceRef departure_anchor_ref; };
// S1/S7同候选真实登记/接受来源；不能靠任意source字符串获选择或搬迁权限。
struct PopulationSelectionAuthority { SourceRef owner_business_ref,release_gate;PopulationSelectionPurpose purpose;std::optional<StableId> required_household_id;Integer maximum_persons=0; };
struct PopulationMaterializationSource { SourceRef actual_source;StableId selection_id,transport_unit_id;SourceRef departure_anchor_ref;Integer effective_tick=0,accepted_scope_revision=0,labor_until_tick=0;std::vector<StableId> release_allocation_ids;WorldMoment occurred_at; };
struct PopulationTransportReleaseSource { SourceRef source;StableId handover_site_id;Integer effective_tick=0;SourceRef location_binding_ref;WorldMoment occurred_at; };
struct PopulationLocationSource { SourceRef location_binding_ref;std::optional<StableId> stationary_site_id; };
class PopulationTransportSourceRead {public:virtual ~PopulationTransportSourceRead()=default;
 virtual Outcome<PopulationSelectionAuthority> read_population_selection_authority(const PopulationSelectionRequest&)const=0;
 virtual Outcome<PopulationMaterializationSource> read_population_transport_registration(const RegisterPopulationTransportFromSelection&)const=0;
 virtual Status validate_population_transport_binding(const PopulationTransportBinding&)const=0;
 virtual Status validate_population_selection_release(const PopulationSelection&,const SourceRef&)const=0;
 virtual Outcome<PopulationTransportReleaseSource> read_population_transport_release(const PopulationTransportBinding&,const SourceRef&)const=0;
 virtual Status validate_population_transport_release(const PopulationTransportBinding&,const SourceRef&)const=0;
};
// 供养交接在原S5源中实际办理；输出只引用真实本期供养、以后义务及位置，不创建钱粮副本。
struct PopulationProvisionHandoff { SourceRef source_ref;std::vector<SourceRef> obligation_continuation_refs;std::vector<StableId> target_provision_binding_ids; };
class PopulationProvisionHandoffWriter {public:virtual ~PopulationProvisionHandoffWriter()=default;virtual Outcome<PopulationProvisionHandoff> handoff_population_provision_in_candidate(CommonStore&,LocalEconomyState&,const PopulationSelection&,const StableId& target_cohort_id,const PopulationMaterializationSource&)=0;virtual Status validate_population_provision_handoff(const PopulationSelectionResult&)const=0;};
struct PopulationRevisionView {CivilianPopulationRef reference;Integer headcount=0,labor_capacity=0;std::optional<SourceRef> condition_source_ref;SourceRef location_binding_ref;std::vector<SourceRef> original_count_sources;};
Outcome<PopulationRevisionView> read_population_revision_source(const LocalEconomyState&,const PopulationTransportState&,const CivilianPopulationRef&);
Outcome<PopulationTransportState> make_initial_population_transport(const CommonStore&,const LocalEconomyState&,Integer selected_version=2);
Outcome<PopulationSelectionQuote> prepare_population_selection(const CommonStore&,const LocalEconomyState&,const PopulationTransportState&,const PopulationTransportSourceRead&,const PopulationSelectionRequest&);
Outcome<StableId> accept_population_selection_in_candidate(CommonStore&,const LocalEconomyState&,PopulationTransportState&,const PopulationTransportSourceRead&,const PopulationSelectionRequest&);
Outcome<StableId> accept_population_selection_in_candidate(CommonStore&,const LocalEconomyState&,PopulationTransportState&,const PopulationTransportSourceRead&,const PopulationSelectionRequest&,const WorldMoment& actual_phase_moment);
Outcome<PopulationSelectionResult> register_population_transport_from_selection_in_candidate(CommonStore&,LocalEconomyState&,PopulationTransportState&,const PopulationTransportSourceRead&,PopulationProvisionHandoffWriter&,const RegisterPopulationTransportFromSelection&);
Outcome<PopulationTransportBinding> read_population_transport_binding(const LocalEconomyState&,const PopulationTransportState&,const StableId&);
Outcome<PopulationLocationSource> read_population_location_source(const LocalEconomyState&,const PopulationTransportState&,const CivilianPopulationRef&);
Outcome<CivilianPopulationRef> read_civilian_population_ref(const LocalEconomyState&,const PopulationTransportState&,const StableId&);
Status release_population_selection_in_candidate(CommonStore&,PopulationTransportState&,const PopulationTransportSourceRead&,const StableId&,const SourceRef&);
Status release_population_transport_in_candidate(CommonStore&,LocalEconomyState&,PopulationTransportState&,const PopulationTransportSourceRead&,const StableId&,const SourceRef&);
Status validate_population_transport(const CommonStore&,const LocalEconomyState&,const PopulationTransportState&,const PopulationTransportSourceRead&,const PopulationProvisionHandoffWriter&);
Status validate_population_transport(const CommonStore&,const LocalEconomyState&,const PopulationTransportState&,const PopulationTransportSourceRead&,const PopulationProvisionHandoffWriter&,const WorldMoment& actual_phase_moment);
BoundaryValue encode_population_transport(const PopulationTransportState&,bool household_purchase_extension=false,Integer selected_version=2);
Outcome<PopulationTransportState> decode_population_transport(const BoundaryValue&,bool household_purchase_extension=false,Integer selected_version=2);
Status visit_population_transport(CodecVisitor&,PopulationTransportState&,bool household_purchase_extension=false,Integer selected_version=2);
Status emit_population_transport(ProjectionWriter&,const PopulationTransportState&,bool household_purchase_extension=false,Integer selected_version=2);
}
