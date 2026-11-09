// 原完整Craft接受与gate的同候选接缝。所有跨域结果必须原owner实际产生并可反读。
#pragma once
#include "craft_materials.hpp"
#include "resident/kernel/read_stamp.hpp"
namespace hanmo::resident {
// Fresh selected craft_jobs/1 only. Never restore missing professional rows by
// constructing an empty root over existing Common Craft facts/actions.
Outcome<CraftJobsState> make_initial_craft_jobs(const CommonStore&,const CommonCatalog&,
 const ItemServiceCatalog&,const CraftRecipeCatalog&,Integer selected_version);
struct CraftFacilityOccupancyContext;
struct CraftCharacterSourceContext;
struct CraftSpatialSourceContext;
struct CraftGateConditionWriteContext;
class CraftCancellationOriginalRead;class CraftCancellationEffectsWrite;
struct CraftGateEffects {std::vector<StableId> actual_fee_refs;};
struct CraftControlEffects {std::vector<MomentOwnerSource> actual_admission_sources;std::vector<StableId> actual_claim_refs;};
enum class CraftControlAdoptionCheck {new_adoption,existing_source};
enum class CraftPaceAdoptionCheck {new_adoption,existing_source};
class CraftJobOriginalRead {
public:
 virtual ~CraftJobOriginalRead()=default;
 // At the selected slice's real from, capture original role/claim/right inputs.
 // Body and spatial heads are captured directly by their native owner readers.
 // This preparation is not evidence that the future service has happened.
 virtual Outcome<WorkServiceIntervalProof> prepare_craft_service_interval(const ActorActionWorkBinding&,const MomentSpan& selected_span)const=0;
 virtual Status validate_prepared_craft_service_source(const WorkServiceIntervalProof&,const MomentOwnerSource&)const=0;
 // At the real to and on cold reads, check the exact previously adopted input
 // refs over the complete half-open span. Never replace them with today's head.
 virtual Status validate_adopted_craft_service_interval(const WorkServiceIntervalProof&)const=0;
 virtual Status validate_work_service_participation_source(const WorkServiceIntervalProof&,const MomentOwnerSource&)const=0;
 // 接受时冻结的学习、技能head、工位使用权、料保管、费用/产物归属完整重验。
 // These original term/control/service readers validate their own inputs and
 // immutable claim references only; they must not call the whole S0 claim
 // validator, which separately invokes the holder source reader.
 virtual Status validate_craft_terms(const CraftTerms&,const SourceRef* actual_acceptance)const=0;
 virtual Status validate_actual_craft_gate_effects(const CraftJob&,const CraftGateApplied&)const=0;
 virtual Status validate_craft_output_destination(const CraftJob&,const SourceRef* actual_output,const WorldMoment&)const=0;
 virtual Status validate_craft_control(const CraftJob&,const CraftControlSource&,CraftControlAdoptionCheck)const=0;
 virtual Status validate_craft_pace_change(const CraftJob&,const CraftPaceChange&,CraftPaceAdoptionCheck)const=0;
 // Current input capture has no future claim/source ID. Historical input
 // validation consumes this actual boundary and its frozen source list.
 virtual Outcome<CraftFacilityEligibility> read_current_craft_facility_eligibility(const CraftJob&)const=0;
 virtual Outcome<CraftFacilityEligibility> read_craft_facility_eligibility_at_boundary(const CraftJob&,const CraftFacilityBoundarySource&)const=0;
 virtual Status validate_actual_craft_completion(const CraftJob&,const CraftCompletionResult&)const=0;
};
class CraftJobEffectsWrite {
public:virtual ~CraftJobEffectsWrite()=default;
 virtual bool bound_to(const CommonStore&,const CraftJobsState&)const noexcept=0;
 virtual Outcome<std::vector<StableId>> adopt_craft_facility_claims_in_candidate(const CraftJob&)=0;
 // 一次完整gate的原工资/服务费，不按穿过的stage数量重复收费。
 // Actual exertion is called directly through the unique S4 writer below.
 virtual Outcome<CraftGateEffects> apply_actual_craft_gate_effects_in_candidate(const CraftJob&,const SourceRef& gate_source,const WorldMoment&)=0;
 // Real facility disposition/readmission; resume also freezes current body/place/right inputs.
 // Consumed WIP retains its actual placement until materialization/cancellation;
 // pause is never permission to clear it or silently release its physical claim.
 virtual Outcome<CraftControlEffects> apply_craft_control_in_candidate(const CraftJob&,const CraftControlSource&)=0;
 virtual Outcome<std::vector<SourceRef>> apply_actual_craft_completion_in_candidate(const CraftJob&,const SourceRef& completion_source,const WorldMoment&)=0;
};
struct CraftJobReadContext {
 CraftMaterialReadContext materials;const ActionElapsedStore& elapsed;
 const CraftJobOriginalRead& original;const CurrentMomentRead& clock;
 const TimeScaleRef& scale;ExactArithmeticBudget arithmetic_budget{};
 const CraftFacilityOccupancyContext* occupancy=nullptr;
 const CraftCancellationOriginalRead* cancellation=nullptr;
 const CraftCharacterSourceContext* character_sources=nullptr;
 const CraftSpatialSourceContext* spatial_sources=nullptr;
};
struct CraftJobWriteContext {
 const CraftJobReadContext& read;const ActionTimeOriginalSourceRead& action_sources;
 const WorkPeriodSourceRead& original_periods;CraftJobEffectsWrite& effects;
 CraftCancellationEffectsWrite* cancellation_effects=nullptr;
 const CraftGateConditionWriteContext* condition_effects=nullptr;
};
struct CraftStartQuote {CraftTerms terms;WorldIdentity identity;std::vector<ReadVersion> inventory_versions;};
ActionTimeRegistration craft_action_time_registration();
Outcome<CraftStartQuote> preview_craft_start(const CraftJobReadContext&,const CraftTerms& prospective);
Outcome<StableId> start_craft_in_candidate(CommonStore&,CraftJobsState&,ActionElapsedStore&,const CraftJobWriteContext&,const CraftStartQuote&);
// Positive service uses craft_interval.hpp: actual-from preparation and actual-to consumption.
Outcome<StableId> apply_craft_work_gate_in_candidate(CommonStore&,CraftJobsState&,ActionElapsedStore&,const CraftJobWriteContext&,const StableId& job_id);
Outcome<const SharedWorkElapsedState*> read_craft_actual_work(const CraftJobReadContext&,const CraftJob&);
Status validate_craft_jobs(const CraftJobReadContext&);
// 单条源反读不回调全S0时间验证，避免elapsed与专业来源互递归。
// S4/S5原效果验证借已成立工量/工序，不反入自身费用或Condition结果。
Outcome<const CraftServiceSpan*> read_craft_actual_service(const CraftJobReadContext&,const StableId&);
// Source-local original positive input for S4/fee causes. It checks immutable
// ownership, Common order, exact positive span and complete participant/ref
// shape without recursively validating the effects that consume this input.
Outcome<const CraftServiceSpan*> read_craft_service_for_original_effect(const CraftJobReadContext&,const StableId&);
Outcome<const CraftGateApplied*> read_craft_gate_for_original_effect(const CraftJobReadContext&,const StableId&);
Outcome<const CraftGateApplied*> read_craft_actual_gate(const CraftJobReadContext&,const StableId&);
Outcome<const CraftQualityResult*> read_craft_actual_quality(const CraftJobReadContext&,const StableId&);
class NativeCraftReservationOwnerRead final:public DomainActivityReservationOwnerRead {
 const CraftJobReadContext& x_;
public:explicit NativeCraftReservationOwnerRead(const CraftJobReadContext&x):x_(x){}
 Outcome<ReservationOwnerView> read_domain_activity_reservation_owner(const ReservationRef&)const override;
};
}
