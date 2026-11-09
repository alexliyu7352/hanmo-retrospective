#pragma once
#include "work_supervision_sources.hpp"
#include "resident/domains/actor_foundation/character_capability.hpp"
#include "resident/domains/actor_foundation/condition_source_read.hpp"
#include "resident/domains/travel/actor_spatial_history.hpp"
#include "resident/domains/local_economy/governance_work/economic_work_window.hpp"
namespace hanmo::resident {
// S6/actual target owner resolves the frozen reference; no caller coordinates,
// current-holder substitution or unrestricted skill number is accepted here.
struct SupervisionScopeView {
 StableId actor_id;MovementAnchor original_target_anchor;std::vector<MomentOwnerSource> actual_sources;
 std::optional<MomentOwnerSource> actual_blocking_source;
};
class SupervisionScopeOriginalRead {
public:virtual ~SupervisionScopeOriginalRead()=default;
 virtual Outcome<SupervisionScopeView> read_start_scope(const WorkEpisode&,const WorldMoment&,const SourceRef* actual_start_cut)const=0;
 virtual Outcome<SupervisionScopeView> read_service_scope(const WorkEpisode&,const SupervisionActionStartSource&,const MomentSpan&)const=0;
 virtual Status validate_actual_release(const WorkEpisode&,const SupervisionActionStartSource&,SupervisionReleaseKind,const MomentOwnerSource&,const WorldMoment&,const WorkOriginalRead&)const=0;
};
struct SupervisionSpatialSources {
 const CommonCatalog& catalog;const MovementExactState& movement;const SpatialCarrierState& carriers;
 const TransportRegistrationState& registrations;const SpatialCarrierSources& originals;ActorBattleSpatialSources battle;
 const MilitarySpatialHandoffState* military_handoffs=nullptr;
};
struct SupervisionNativeContext {
 const CommonStore& common;const TaskState& tasks;const CharacterConditionSources& body;
 SupervisionSpatialSources spatial;const SupervisionScopeOriginalRead& scopes;const CurrentMomentRead& clock;
 const WorkSkillInputsRead* skill_inputs=nullptr;
};
class NativeSupervisionEligibilityRead final:public SupervisionEligibilityRead {
 const SupervisionNativeContext& context_;
public:explicit NativeSupervisionEligibilityRead(const SupervisionNativeContext&x):context_(x){}
 Outcome<SupervisionStartInputs> read_current_start_inputs(const WorkEpisode&,const WorldMoment&)const override;
 Status validate_historical_start_inputs(const WorkEpisode&,const SupervisionActionStartSource&)const override;
 Status validate_actual_service_span(const WorkEpisode&,const SupervisionActionStartSource&,const MomentSpan&)const override;
 Status validate_actual_release(const WorkEpisode&,const SupervisionActionStartSource&,SupervisionReleaseKind,const MomentOwnerSource&,const WorldMoment&,const WorkOriginalRead&)const override;
};
struct SupervisionEconomicSources {
 const LocalEconomyState& economy;const LocalEconomyCatalog& catalog;
 const EconomicWorkCatalog& rules;const EconomicWorkSourcesState& state;const ProductionInputSourceRead& originals;Integer units_per_half_day=0;
};
// True applied S5 window + actual S1 stationary presence + original S4 body
// slices. Reads never close a segment, write knowledge or award professional effects.
Outcome<WorkPeriodRead> read_supervised_economic_period(const SupervisionNativeContext&,const SupervisionSourceContext&,const SupervisionEconomicSources&,const WorkEpisode&,const WorkEconomicPeriodSource&);
Outcome<WorkCompletionRead> read_supervised_economic_completion(const SupervisionNativeContext&,const SupervisionSourceContext&,const SupervisionEconomicSources&,const WorkEpisode&,const std::vector<StableId>& actual_receipts,const WorldMoment&);
// Original S5 close first, then the one original S3 receipt writer. Uses actual
// existing Work/Action references; neither the request nor a planned allocation
// can assert a quantity or qualification.
Outcome<StableId> consume_supervised_economic_window_in_candidate(CommonStore&,TaskState&,const SupervisionNativeContext&,const SupervisionSourceContext&,const SupervisionEconomicSources&,EconomicProductionContext&,WorkExpenseConsumer&,const StableId& work_id,const MomentSpan& scheduled_window);
}
