// Fixed S3 §13.2: the original positive-duration appointment, not a zero-time substitute.
#pragma once
#include "personal_appointment.hpp"
#include "resident/kernel/action_time_validation.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/domains/family/family_life_exact.hpp"
#include "resident/domains/actor_foundation/character_presence_v2.hpp"
namespace hanmo::resident {
struct PersonalAppointmentTime2Context {
 const CommonStore& common;const InteractionState& interaction;const InteractionCatalog& definitions;
 const PersonalAppointmentCatalog& catalog;const PersonalAppointmentState& state;
 const ActionElapsedStore& elapsed;const TimeScaleRef& scale;const CatalogIdentity& identity;
 const CurrentMomentRead& clock;const FamilyDeathPolicy& life_policy;
 const FamilyDeathState& deaths;const FamilyLifeCauseSources4& life_causes;
 const AuthoredActorFoundationState& body;const ActorFoundationCatalog& body_catalog;
 const FamilyCatalog& family_catalog;
 const BornCharacterSourceRead* born_sources=nullptr;
};
ActionTimeRegistration personal_appointment_action_time_registration();
Outcome<const S3EventTime*> read_personal_appointment_event_time(const PersonalAppointmentTime2Context&,const S3PersonalAppointmentRecord&,S3EventRole);
Outcome<WorldMoment> read_personal_appointment_source_moment(const PersonalAppointmentTime2Context&,const SourceRef&,Integer schema);
Status validate_personal_appointment_time2(const PersonalAppointmentTime2Context&);
Outcome<StableId> read_personal_appointment_status2(const PersonalAppointmentTime2Context&,const StableId& appointment_id);
// Completion retains the original two participants, actual session and failure rules.
// The phase/death owner releases the one Common Action after this actual result.
Outcome<PersonalAppointmentCompletion> finish_personal_appointment_time2_in_candidate(CommonStore&,PersonalAppointmentState&,ActionElapsedStore&,const PersonalAppointmentTime2Context&,const InteractionAccess&,const ActionCoreView&,const std::optional<DeathRef>& actual_death={});
Outcome<const PersonalAppointmentLifeSource*> read_personal_appointment_life_source(const PersonalAppointmentTime2Context&,const StableId& result_id);
Status validate_personal_appointment_life_sources(const PersonalAppointmentTime2Context&);
// The actual Family death candidate invokes this before its outer validation.
// Returns only genuinely ended Actions; the outer owner invokes its existing
// unique release_action before publishing or performing complete cold validation.
Outcome<std::vector<StableId>> apply_personal_appointment_death_in_candidate(CommonStore&,PersonalAppointmentState&,ActionElapsedStore&,const PersonalAppointmentTime2Context&,const InteractionAccess&,const DeathRef&);
class PersonalAppointmentActionTimeRead final:public ActionTimeOriginalSourceRead {
 const PersonalAppointmentTime2Context& x_;
public:explicit PersonalAppointmentActionTimeRead(const PersonalAppointmentTime2Context&x):x_(x){}
 bool bound_to(const CommonStore&c,const PersonalAppointmentState&s)const noexcept{return &x_.common==&c&&&x_.state==&s;}
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
Status bind_personal_appointment_time2_command_result(CommonStore&,PersonalAppointmentState&,const PersonalAppointmentTime2Context&,const std::vector<StableId>& actual_new_sources,const CommitResult&);
// These are invoked by the original successful writers, in the same candidate.
Status record_personal_appointment_zero_time_in_candidate(PersonalAppointmentState&,const PersonalAppointmentTime2Context&,const SourceRef& actual_new_fact);
Status record_personal_appointment_started_time_in_candidate(CommonStore&,PersonalAppointmentState&,ActionElapsedStore&,const PersonalAppointmentTime2Context&,const StableId& original_start_fact);
Status record_personal_appointment_finished_time_in_candidate(CommonStore&,PersonalAppointmentState&,ActionElapsedStore&,const PersonalAppointmentTime2Context&,const StableId& original_finished_fact,const std::optional<DeathRef>& actual_death={});
}
