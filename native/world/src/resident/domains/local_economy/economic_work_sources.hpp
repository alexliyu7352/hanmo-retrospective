#include "resident/kernel/world_timeline.hpp"
// D5.4/RWM：原生产输入的精确累计，不是另一生产器、时间钟或产物余额。
#pragma once
#include "local_economy_model.hpp"
#include "resident/domains/actor_foundation/actor_foundation_refs.hpp"
#include "resident/kernel/source_ref.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "hanmo/resident/world_moment.hpp"
namespace hanmo::resident {
class ProjectionWriter;
enum class EconomicWorkKind { agronomy_operation,industrial_operation,facility_construction };
struct WorkRef { StableId owner_domain;EconomicWorkKind kind=EconomicWorkKind::agronomy_operation;StableId work_id; };
struct CommonActionActivityRef { StableId action_id; };
struct OrganizationActivityRef { StableId org_activity_id; };
struct ProductionCycleActivityRef { StableId unit_id;Integer cycle_start_tick=0,due_tick=0; };
struct ConstructionStageActivityRef { StableId construction_id;Integer stage_ordinal=0; };
using SourceActivityRef=std::variant<CommonActionActivityRef,OrganizationActivityRef,ProductionCycleActivityRef,ConstructionStageActivityRef>;
enum class EconomicWorkerRole { worker,foreman,operator_role };
struct EconomicNamedWorker { StableId actor_id;EconomicWorkerRole role_kind=EconomicWorkerRole::worker; };
enum class EconomicWorkStatus { applied_work,completed_stage,interrupted,no_work };
struct EconomicWorkPeriodView2 { SourceRef receipt_ref;WorkRef work_ref;SourceActivityRef source_activity_ref;MomentSpan window;Location site_or_anchor_ref;std::vector<EconomicNamedWorker> named_participants;std::vector<StableId> cohort_labor_refs;CanonicalFraction actual_worker_halfdays;std::vector<SourceRef> segment_source_refs,actual_participant_coverage_refs,material_effect_refs;std::optional<SourceRef> stage_result_ref;EconomicWorkStatus status=EconomicWorkStatus::no_work;std::optional<SourceRef> reason;Integer rule_version=0; };
// 跨原cycle窗口保留逐源原活动，不伪造一个跨期SourceActivityRef。
struct EconomicWorkWindowRead2 { WorkRef work_ref;MomentSpan window;CanonicalFraction actual_worker_halfdays;std::vector<EconomicWorkPeriodView2> source_parts; };
struct ProductionSegmentSource { StableId unit_id,cycle_source_ref;WorldMoment from,to;std::vector<SourceRef> condition_source_refs,asset_use_source_refs,participation_refs,actual_supply_refs;Integer batch_capacity=0,labor_rate_numerator=0,labor_rate_denominator=1; };
struct AccumulatedProductionState2 { StableId cycle_source_ref,unit_id;Integer start_tick=0,due_tick=0;WorldMoment last_accounted_at;std::vector<SourceRef> active_input_source_refs;CanonicalFraction capacity_unit_sum,labor_unit_numerator;Integer labor_denominator=1;StableId original_labor_remainder_ref,last_segment_key;std::optional<StableId> settlement_receipt_ref;ProductionSegmentSource active_input;std::vector<StableId> active_labor_allocation_refs;Location active_anchor; };
struct EconomicProductionProfile { StableId unit_id;EconomicWorkKind work_kind=EconomicWorkKind::industrial_operation;Integer rule_version=1,worker_halfdays_per_batch=1,labor_denominator=1; };
struct EconomicWorkCatalog { OrderedTable<EconomicProductionProfile> production_profiles; };
// All selected immutable rows are checked against the original author catalog.
// This does not read current cycles or infer an unselected production profile.
Status validate_economic_work_catalog(const EconomicWorkCatalog&,const LocalEconomyCatalog&);
struct AppliedProductionSegment { StableId id;ProductionSegmentSource source;SourceRef close_cause;CanonicalFraction capacity_units_added,labor_numerator_added,actual_worker_halfdays;std::vector<StableId> actual_labor_allocation_refs;Location actual_anchor; };
struct EconomicLaborRemainderSource { StableId id,unit_id;CanonicalFraction value;std::optional<StableId> previous_source_ref,settlement_receipt_ref; };
struct EconomicProductionSettlementSource { StableId receipt_id,cycle_source_ref;Integer capacity_limit=0,labor_limit=0;CanonicalFraction actual_worker_halfdays,labor_remainder;StableId remainder_before_ref,remainder_after_ref; };
// schema 3 only: the original calendar opening, never an independently advanced clock.
// The IDs are captured once before the period; closing cannot reconstruct them.
struct EconomicPeriodOpening { StableId source_id,period_id;WorldMoment opening_at;Integer through_tick=0;std::vector<StableId> existing_activity_ids,existing_action_ids; };
enum class EconomicWorkSourceSchema : Integer { production_sources2=2,production_calendar3=3 };
struct EconomicWorkSourcesState { OrderedTable<AccumulatedProductionState2> cycles;OrderedTable<StableId> active_cycles_by_unit;OrderedTable<AppliedProductionSegment> segments;OrderedTable<EconomicLaborRemainderSource> labor_remainders;OrderedTable<EconomicProductionSettlementSource> settlements;std::optional<ScenarioOriginRef> calendar_origin;OrderedTable<EconomicPeriodOpening> period_openings; };
// 唯一生产容量/真实参与来源，读调用不结段。外部变更必须先close旧段再装新源。
struct ProductionInputRead { ProductionSegmentSource source;std::vector<StableId> actual_labor_allocation_refs;Location actual_anchor; };
class ProductionInputSourceRead {
public:virtual ~ProductionInputSourceRead()=default;
 virtual Outcome<ProductionInputRead> read_production_segment_inputs(const StableId& unit,const StableId& cycle,const WorldMoment& from,const WorldMoment& until)const=0;
 // Only the original successful change writer passes its actual immutable source.
 virtual Outcome<ProductionInputRead> read_production_inputs_after_change(const StableId& unit,const StableId& cycle,const WorldMoment& from,const WorldMoment& until,const MomentOwnerSource&)const=0;
 // Reconstruct the frozen participation/anchor too, not just its numeric rate.
 virtual Outcome<ProductionInputRead> read_validated_production_segment_source(const ProductionSegmentSource&)const=0;
 virtual Status validate_production_segment_source(const ProductionSegmentSource&)const=0;
 virtual Status validate_production_input_change(const StableId& unit,const WorldMoment&,const SourceRef&)const=0;
 // Earliest actual locked allocation/supply rule boundary after adopted inputs.
 // This pure projection neither closes a segment nor creates a due source.
 virtual Outcome<std::optional<WorldMoment>> read_next_production_rule_boundary(const StableId& unit,const StableId& cycle)const=0;
};
struct EconomicProductionLimits { Integer capacity_limit=0,labor_limit=0;CanonicalFraction actual_worker_halfdays,labor_remainder;StableId cycle_source_ref,prior_remainder_ref; };
Outcome<StableId> open_production_cycle_in_candidate(CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,const EconomicWorkCatalog&,EconomicWorkSourcesState&,const ProductionInputSourceRead&,const StableId& unit,const WorldMoment& actual_start,const CurrentMomentRead&);
Outcome<std::optional<StableId>> close_production_input_segment_in_candidate(CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,const EconomicWorkCatalog&,EconomicWorkSourcesState&,const ProductionInputSourceRead&,const StableId& unit,const WorldMoment& at,const SourceRef& change_source,Integer units_per_half_day,ExactArithmeticBudget={});
Status install_production_input_source_in_candidate(const LocalEconomyState&,EconomicWorkSourcesState&,const ProductionInputSourceRead&,const StableId& unit,const WorldMoment& at);
Status install_changed_production_input_source_in_candidate(EconomicWorkSourcesState&,const ProductionInputSourceRead&,const StableId& unit,const WorldMoment& at,const MomentOwnerSource& actual_change);
Outcome<EconomicProductionLimits> read_accumulated_production_limits(const LocalEconomyState&,const LocalEconomyCatalog&,const EconomicWorkCatalog&,const EconomicWorkSourcesState&,const StableId& unit,Integer units_per_half_day,ExactArithmeticBudget={});
Status settle_accumulated_production_in_candidate(CommonStore&,const LocalEconomyState&,EconomicWorkSourcesState&,const EconomicProductionLimits&,const StableId& original_production_receipt);
Outcome<EconomicWorkWindowRead2> read_economic_work_window(const CommonStore&,const LocalEconomyState&,const EconomicWorkCatalog&,const EconomicWorkSourcesState&,const ProductionInputSourceRead&,const WorkRef&,const MomentSpan&,const WorldIdentity& expected_identity,ExactArithmeticBudget={});
// Borrow the actual schema-2 source after its original cycle/inputs/closing cause
// and exact integral are checked. This is not a current-production projection.
Outcome<const AppliedProductionSegment*> read_verified_production_input_segment(const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,const EconomicWorkCatalog&,const EconomicWorkSourcesState&,const ProductionInputSourceRead&,const SourceRef&,Integer source_schema,Integer units_per_half_day,ExactArithmeticBudget={});
Status validate_economic_work_sources(const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,const EconomicWorkCatalog&,const EconomicWorkSourcesState&,const ProductionInputSourceRead&,Integer units_per_half_day,ExactArithmeticBudget={});
struct EconomicWorkValidationContext {const EconomicWorkCatalog& catalog;const EconomicWorkSourcesState& sources;const ProductionInputSourceRead& inputs;const CurrentMomentRead& clock;Integer units_per_half_day=0;};
Status validate_economic_production_source_history(const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,const EconomicWorkValidationContext&);
// Only a selected real cycle settlement replaces the legacy capacity comparison.
Outcome<std::optional<Integer>> read_original_production_receipt_capacity(const EconomicWorkValidationContext&,const ProductionReceipt&);
class EconomicOriginalPaymentRead;
struct EconomicProductionContext { const EconomicWorkCatalog& catalog;EconomicWorkSourcesState& state;const ProductionInputSourceRead& inputs;const CurrentMomentRead& clock;Integer units_per_half_day=0;const EconomicOriginalPaymentRead* original_payments=nullptr; };
// Ephemeral prepared old interval. Only the original writer can create this
// token; it is consumed once after its real change source has been committed to
// the same unpublished candidate. No synthetic closing fact is needed at zero.
struct ProductionInputCloseAccess;
class PreparedProductionInputClose {
 friend struct ProductionInputCloseAccess;
 const CommonStore* common_=nullptr;const EconomicWorkSourcesState* state_=nullptr;
 AccumulatedProductionState2 before_;ProductionSegmentSource segment_;
 CanonicalFraction capacity_added_,labor_added_,half_days_,next_capacity_,next_labor_;
 bool valid_=false,positive_=false;
public:
 PreparedProductionInputClose()=default;
 PreparedProductionInputClose(const PreparedProductionInputClose&)=delete;
 PreparedProductionInputClose& operator=(const PreparedProductionInputClose&)=delete;
 PreparedProductionInputClose(PreparedProductionInputClose&&)=default;
 PreparedProductionInputClose& operator=(PreparedProductionInputClose&&)=default;
};
Outcome<PreparedProductionInputClose> prepare_production_input_close(
 const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,const EconomicProductionContext&,
 const StableId& unit,const WorldMoment& at,ExactArithmeticBudget={});
Outcome<std::optional<StableId>> apply_prepared_production_input_close_in_candidate(
 CommonStore&,EconomicProductionContext&,PreparedProductionInputClose&&,
 const SourceRef& actual_change,ExactArithmeticBudget={});
Outcome<LocalEconomyEffects> step_local_economy_exact_in_candidate(CommonStore&,LocalEconomyState&,const LocalEconomyCatalog&,const OrganizationRead&,const ActivityRead&,const DynamicStepInput&,const HouseholdProvisionManagementContext*,ProductionDisruptionContext*,EconomicTakeoverPhaseContext*,const EstateEmploymentContext*,MarketBusinessCaptureServices*,const PrivateShopResponsibilitySourceContext*,EconomicProductionContext&);
Outcome<BoundaryValue> encode_economic_work_sources(const EconomicWorkSourcesState&,EconomicWorkSourceSchema=EconomicWorkSourceSchema::production_sources2);
Outcome<EconomicWorkSourcesState> decode_economic_work_sources(const BoundaryValue&,EconomicWorkSourceSchema=EconomicWorkSourceSchema::production_sources2);
Status visit_economic_work_sources(CodecVisitor&,EconomicWorkSourcesState&,EconomicWorkSourceSchema=EconomicWorkSourceSchema::production_sources2);
Outcome<BoundaryValue> encode_economic_work_catalog(const EconomicWorkCatalog&);
Outcome<EconomicWorkCatalog> decode_economic_work_catalog(const BoundaryValue&);
Status emit_economic_work_sources(ProjectionWriter&,const EconomicWorkSourcesState&,EconomicWorkSourceSchema=EconomicWorkSourceSchema::production_sources2);
}
