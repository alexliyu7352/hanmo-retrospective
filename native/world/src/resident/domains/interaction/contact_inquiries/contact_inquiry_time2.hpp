// Original one-half-day inquiry. The original start is the knowledge cut;
// completion never imports observations made after that accepted start.
#pragma once
#include "contact_inquiry.hpp"
#include "resident/kernel/action_time_validation.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/domains/family/family_life_exact.hpp"
#include "resident/domains/actor_foundation/character_presence_v2.hpp"
namespace hanmo::resident {
struct ContactInquiryTime2Context {
 const CommonStore& common;const InteractionState& interaction;const InteractionCatalog& definitions;
 const ContactInquiryCatalog& catalog;const ContactInquiryState& state;
 const KnownContactLocationAccess& knowledge;
 const ActionElapsedStore& elapsed;const TimeScaleRef& scale;const CatalogIdentity& identity;
 const CurrentMomentRead& clock;const FamilyDeathPolicy& life_policy;const FamilyDeathState& deaths;
 const FamilyLifeCauseSources4& life_causes;const AuthoredActorFoundationState& body;
 const ActorFoundationCatalog& body_catalog;const FamilyCatalog& family_catalog;
 const BornCharacterSourceRead* born_sources=nullptr;
};
ActionTimeRegistration contact_inquiry_action_time_registration();
Outcome<const S3EventTime*> read_contact_inquiry_event_time(const ContactInquiryTime2Context&,const S3ContactInquiryRecord&,S3EventRole);
Outcome<WorldMoment> read_contact_inquiry_source_moment(const ContactInquiryTime2Context&,const SourceRef&,Integer schema);
Status validate_contact_inquiry_time2(const ContactInquiryTime2Context&);
Outcome<ContactInquiryCompletion> finish_contact_inquiry_time2_in_candidate(CommonStore&,ContactInquiryState&,ActionElapsedStore&,const ContactInquiryTime2Context&,const InteractionAccess&,const ActionCoreView&,const std::optional<DeathRef>& actual_death={});
Outcome<const ContactInquiryLifeSource*> read_contact_inquiry_life_source(const ContactInquiryTime2Context&,const StableId&);
Status validate_contact_inquiry_life_sources(const ContactInquiryTime2Context&);
Outcome<std::vector<StableId>> apply_contact_inquiry_death_in_candidate(CommonStore&,ContactInquiryState&,ActionElapsedStore&,const ContactInquiryTime2Context&,const InteractionAccess&,const DeathRef&);
class ContactInquiryActionTimeRead final:public ActionTimeOriginalSourceRead {
 const ContactInquiryTime2Context& x_;
public:explicit ContactInquiryActionTimeRead(const ContactInquiryTime2Context&x):x_(x){}
 bool bound_to(const CommonStore&c,const ContactInquiryState&s)const noexcept{return &x_.common==&c&&&x_.state==&s;}
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
Status record_contact_inquiry_started_time_in_candidate(CommonStore&,ContactInquiryState&,ActionElapsedStore&,const ContactInquiryTime2Context&,const StableId&);
Status record_contact_inquiry_finished_time_in_candidate(CommonStore&,ContactInquiryState&,ActionElapsedStore&,const ContactInquiryTime2Context&,const StableId&,const std::optional<DeathRef>& actual_death={});
}
