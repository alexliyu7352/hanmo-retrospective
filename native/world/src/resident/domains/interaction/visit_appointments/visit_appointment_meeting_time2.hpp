// Fixed S3 §4.2/13: the original invitation meeting keeps its real positive duration.
#pragma once
#include "visit_appointment_meeting.hpp"
#include "visit_appointment_life.hpp"
#include "resident/kernel/action_time_validation.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/domains/family/family_life_exact.hpp"
#include "resident/domains/actor_foundation/character_presence_v2.hpp"
namespace hanmo::resident {
struct VisitAppointmentMeetingTime2Context {
 const CommonStore& common;const InteractionState& interaction;const InteractionCatalog& definitions;
 const PersonalAppointmentCatalog& catalog;const VisitAppointmentState& invitations;const VisitAppointmentMeetingState& state;
 const PersonalAppointmentState& personal;const VisitAppointmentHistoricalSources& invitation_history;const VisitAppointmentLifeContext& invitation_life;
 const ActionElapsedStore& elapsed;const TimeScaleRef& scale;const CatalogIdentity& identity;
 const CurrentMomentRead& clock;const FamilyDeathPolicy& life_policy;
 const FamilyDeathState& deaths;const FamilyLifeCauseSources4& life_causes;
 const AuthoredActorFoundationState& body;const ActorFoundationCatalog& body_catalog;
 const FamilyCatalog& family_catalog;
 const BornCharacterSourceRead* born_sources=nullptr;
};
ActionTimeRegistration visit_appointment_meeting_action_time_registration();
Outcome<const S3EventTime*> read_visit_appointment_meeting_event_time(const VisitAppointmentMeetingTime2Context&,const S3VisitAppointmentMeetingRecord&,S3EventRole);
Outcome<WorldMoment> read_visit_appointment_meeting_source_moment(const VisitAppointmentMeetingTime2Context&,const SourceRef&,Integer schema);
Status validate_visit_appointment_meeting_time2(const VisitAppointmentMeetingTime2Context&);

// Completion retains the original two participants, actual session and failure rules.
// The phase/death owner releases the one Common Action after this actual result.
Outcome<VisitAppointmentMeetingCompletion> finish_visit_appointment_meeting_time2_in_candidate(CommonStore&,VisitAppointmentMeetingState&,ActionElapsedStore&,const VisitAppointmentMeetingTime2Context&,const InteractionAccess&,const ActionCoreView&,const std::optional<DeathRef>& actual_death={});
Outcome<const VisitAppointmentMeetingLifeSource*> read_visit_appointment_meeting_life_source(const VisitAppointmentMeetingTime2Context&,const StableId& result_id);
Status validate_visit_appointment_meeting_life_sources(const VisitAppointmentMeetingTime2Context&);
// The actual Family death candidate invokes this before its outer validation.
// Returns only genuinely ended Actions; the outer owner invokes its existing
// unique release_action before publishing or performing complete cold validation.
Outcome<std::vector<StableId>> apply_visit_appointment_meeting_death_in_candidate(CommonStore&,VisitAppointmentMeetingState&,ActionElapsedStore&,const VisitAppointmentMeetingTime2Context&,const InteractionAccess&,const DeathRef&);
class VisitAppointmentMeetingActionTimeRead final:public ActionTimeOriginalSourceRead {
 const VisitAppointmentMeetingTime2Context& x_;
public:explicit VisitAppointmentMeetingActionTimeRead(const VisitAppointmentMeetingTime2Context&x):x_(x){}
 bool bound_to(const CommonStore&c,const VisitAppointmentMeetingState&s)const noexcept{return &x_.common==&c&&&x_.state==&s;}
 Status enumerate_action_time_owners(const ActionTimeRegistration&,ActionTimeOwnerIdSink&)const override;
 Outcome<ActionTimeOwnerProof> read_action_time_owner(const ActionTimeRegistration&,const StableId&)const override;
 Outcome<WorldMoment> read_action_time_source(const ElapsedTimeSource&)const override;
 Status validate_fixed_action_sources(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const ActionElapsedState&,const std::vector<FixedServiceTransition>&)const override;
 Status validate_shared_action_sources(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const SharedWorkElapsedState&)const override;
 Outcome<RunStop> read_action_continuation_stop(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const WorldMoment&)const override;
 Outcome<OriginalActionRemaining> read_original_action_remaining(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const WorldMoment&)const override;
 Outcome<StableId> read_gate_result_id(const SharedWorkGateResult&)const override;
 Outcome<SharedWorkGateResult> read_original_gate_result(const StableId&)const override;
 Status validate_original_gate_result(const SharedWorkGateResult&)const override;
};
// Same original command results are sealed before this endpoint. It binds only
// the writer's existing zero-step event; it never allocates a result identity.
Status bind_visit_appointment_meeting_time2_command_result(CommonStore&,VisitAppointmentMeetingState&,const VisitAppointmentMeetingTime2Context&,const std::vector<StableId>& actual_new_sources,const CommitResult&);
// These are invoked by the original successful writers, in the same candidate.
Status record_visit_appointment_meeting_zero_time_in_candidate(VisitAppointmentMeetingState&,const VisitAppointmentMeetingTime2Context&,const SourceRef& actual_new_fact);
Status record_visit_appointment_meeting_started_time_in_candidate(CommonStore&,VisitAppointmentMeetingState&,ActionElapsedStore&,const VisitAppointmentMeetingTime2Context&,const StableId& original_start_fact);
Status record_visit_appointment_meeting_finished_time_in_candidate(CommonStore&,VisitAppointmentMeetingState&,ActionElapsedStore&,const VisitAppointmentMeetingTime2Context&,const StableId& original_finished_fact,const std::optional<DeathRef>& actual_death={});
}
