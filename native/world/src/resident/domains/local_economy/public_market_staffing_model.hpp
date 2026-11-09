// 经济07/10唯一值守和原劳动历史型。当前人数/劳力仍只在原LE；组织状态仍S6。
#pragma once
#include "local_economy_model.hpp"
#include "population_source_ref.hpp"
#include "resident/domains/family/household_control.hpp"
#include "resident/domains/actor_foundation/actor_foundation_refs.hpp"
#include "resident/kernel/world_timeline.hpp"
namespace hanmo::resident {
struct PublicStaffEmployment { SourceRef accepted_wage_contract_ref;std::vector<StableId> provision_binding_refs; };
struct PublicStaffAcceptedDuty { SourceRef original_duty_terms_ref,explicit_no_wage_consent_ref;std::vector<StableId> provision_binding_refs; };
using PublicStaffCompensationBasis=std::variant<PublicStaffEmployment,PublicStaffAcceptedDuty>;
struct PublicMarketStaffingTerms {
 ActorFoundationRuleRef terms_ref;Integer terms_revision=1;ActorFoundationRuleRef market_definition_ref;
 StableId original_org_activity_terms_ref,organization_ref,responsible_office_ref;CivilianPopulationRef cohort_ref;
 Integer workers=0,from_tick=0,until_tick=0;PublicStaffCompensationBasis compensation_basis;
 SourceRef original_opening_authorization_ref,original_labor_authority_ref;ActorFoundationRuleRef continuity_rule_ref;
};
struct PublicMarketStaffingOffer { StableId offer_id;PublicMarketStaffingTerms terms;SourceRef employer_acceptance_ref,local_contact_source_ref;WorldMoment issued_at,expires_at; };
struct PublicMarketStaffingConsent { StableId consent_id,offer_ref;Integer terms_revision=0;CivilianPopulationRef cohort_ref;HouseholdProvisionControlProof controller_authority_ref;SourceRef local_contact_source_ref;WorldMoment consented_at; };
struct PublicMarketStaffingReleaseConsent { StableId consent_id,binding_ref;Integer terms_revision=0,release_tick=0;HouseholdProvisionControlProof controller_authority_ref;SourceRef local_contact_source_ref;WorldMoment consented_at; };
struct PublicMarketStaffingBinding { StableId binding_id,offer_ref,consent_ref,original_org_activity_ref,original_labor_allocation_ref;SourceRef accepted_event_ref,labor_origin_ref; };
enum class PublicMarketStaffingEventKind {accepted,qualification_changed,stop_at_boundary,completed};
struct PublicMarketStaffingEvent { StableId event_id,binding_ref;Integer event_ordinal=0;WorldMoment occurred_at;PublicMarketStaffingEventKind kind=PublicMarketStaffingEventKind::accepted;SourceRef cause_ref; };
struct PublicMarketLaborOrigin {
 StableId origin_id;Integer source_schema=1;StableId binding_ref;LaborAllocation original_allocation;MomentSpan committed_span;
 CivilianPopulationRef population_source_ref;SourceRef location_source_ref;std::vector<SourceRef> eligibility_source_refs;
 SourceRef accepted_labor_authority_ref,approved_org_fact_ref;WorldMoment accepted_at;
};
struct PublicMarketEligibilityChanged { CivilianPopulationRef before_population_source_ref,after_population_source_ref;std::vector<SourceRef> actual_location_or_condition_source_refs;SourceRef qualification_change_cause_ref; };
struct PublicMarketAllocationAdjusted { SourceRef original_population_change_ref;LaborAllocation before_allocation;std::optional<LaborAllocation> after_allocation;MomentSpan effective_span; };
enum class PublicMarketLaborReleaseReason {completed,mutually_cancelled};
struct PublicMarketLaborReleased { SourceRef original_org_boundary_fact_ref;PublicMarketLaborReleaseReason release_reason=PublicMarketLaborReleaseReason::completed;MomentSpan ended_committed_span; };
using PublicMarketLaborChange=std::variant<PublicMarketEligibilityChanged,PublicMarketAllocationAdjusted,PublicMarketLaborReleased>;
struct PublicMarketLaborTransition { StableId transition_id;Integer source_schema=1;SourceRef origin_ref;Integer transition_ordinal=0;std::optional<SourceRef> previous_transition_ref;WorldMoment occurred_at;PublicMarketLaborChange change; };
struct PublicMarketEffectiveAllocationSegment { MomentSpan span;Integer workers=0;SourceRef source_ref; };
struct PublicMarketEligibleServiceSegment { MomentSpan span;Integer eligible_workers=0;std::vector<SourceRef> actual_qualification_source_refs; };
struct PublicMarketStaffingLaborHistoryView { SourceRef origin_ref;StableId original_allocation_id;MomentSpan committed_span;std::vector<PublicMarketEffectiveAllocationSegment> effective_allocation_segments;std::vector<PublicMarketEligibleServiceSegment> eligible_service_segments;std::optional<SourceRef> release_transition_ref; };
}
