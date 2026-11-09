// P24.1 / FH5.3B: actual command start and its same-candidate sealed-result tail.
#pragma once
#include "care_handover_command_source.hpp"
#include "care_handover_time.hpp"
namespace hanmo::resident {
struct CommitResult;
struct StartCareHandoverRequest {StableId accepted_care_terms_ref;};
struct CareHandoverCurrentStart {
 std::vector<StableId> worker_actor_ids;
 ActorFoundationRuleRef handover_rule_ref,transition_rule_ref;
 std::vector<MomentOwnerSource> original_input_sources;
};
struct CareHandoverStartedSources {
 std::vector<MomentOwnerSource> actual_participation_source_refs,facility_use_binding_refs,authority_source_refs;
};
// The actual source owners establish consent, route, lawful location, accepted
// fees/facility use and worker participation. Empty costs require the original
// rule's positive validation too. These ports cannot default to success.
class CareHandoverStartOriginalRead {
public:virtual ~CareHandoverStartOriginalRead()=default;
 virtual bool bound_to(const CommonStore&)const noexcept=0;
 virtual Outcome<CareHandoverCurrentStart> read_current_start(const StableId& actor,
  const AcceptedCaregiver&,const WorldMoment&)const=0;
 // Source-local: checks these original admissions without requiring command
 // seal, FSL adoption, or recursively calling the whole Care validator.
 virtual Status validate_started_sources(const CareHandoverExecution&,
  const AcceptedCaregiver&,const CareHandoverExecutionBoundary1&)const=0;
};
class CareHandoverStartOriginalWrite:public CareHandoverStartOriginalRead {
public:
 // Actual Action already exists. Applies only the original source owners'
 // accepted reservations/admissions; never writes Care's row or any body value.
 virtual Outcome<CareHandoverStartedSources> reserve_started_resources_in_candidate(
  CommonStore&,const CareHandoverExecution&,const AcceptedCaregiver&,
  const CareHandoverCurrentStart&)=0;
};
struct CareHandoverStartContext {
 const CareHandoverCommandSourceContext& command_sources;
 const CareHandoverStartOriginalRead& originals;
 const CurrentReadStampRead& reads;const ReadStampAuthority& stamps;
 const DomainReadVersionSource& versions;
};
class PreparedCareHandoverStart;
class PendingCareHandoverStart;
Outcome<PreparedCareHandoverStart> prepare_care_handover_start(
 const CareHandoverStartContext&,const StableId& actor,const StartCareHandoverRequest&);
Outcome<PendingCareHandoverStart> apply_care_handover_start_in_candidate(
 CommonStore&,OrderedTable<CareHandoverExecution>&,const CareHandoverStartContext&,
 CareHandoverStartOriginalWrite&,PreparedCareHandoverStart&&);
// The only mutable proof is private and short-lived. It cannot be decoded or
// used with another Common/world, accepted row, source owner, or actual moment.
class PreparedCareHandoverStart {
 const CommonStore* common_=nullptr;const OrderedTable<CareHandoverExecution>* executions_=nullptr;
 const CareHandoverStartOriginalRead* originals_=nullptr;
 StableId actor_,accepted_,requirement_,requirement_transition_;WorldMoment at_;
 Integer duration_=0;CareHandoverCurrentStart original_;
 std::vector<FrozenFacilityCapabilityInputs> workers_;std::shared_ptr<const ReadStamp> stamp_;
 friend Outcome<PreparedCareHandoverStart> prepare_care_handover_start(const CareHandoverStartContext&,const StableId&,const StartCareHandoverRequest&);
 friend Outcome<PendingCareHandoverStart> apply_care_handover_start_in_candidate(CommonStore&,OrderedTable<CareHandoverExecution>&,const CareHandoverStartContext&,CareHandoverStartOriginalWrite&,PreparedCareHandoverStart&&);
public:
 PreparedCareHandoverStart()=default;PreparedCareHandoverStart(const PreparedCareHandoverStart&)=delete;
 PreparedCareHandoverStart& operator=(const PreparedCareHandoverStart&)=delete;
 PreparedCareHandoverStart(PreparedCareHandoverStart&&)=default;PreparedCareHandoverStart& operator=(PreparedCareHandoverStart&&)=default;
 Integer half_days()const noexcept{return duration_;}
 const std::vector<StableId>& participant_actor_ids()const noexcept{return original_.worker_actor_ids;}
};
class PendingCareHandoverStart {
 const CommonStore* common_=nullptr;const OrderedTable<CareHandoverExecution>* executions_=nullptr;
 const CareHandoverStartOriginalRead* originals_=nullptr;
 StableId actor_,handover_,action_;WorldMoment at_;CareHandoverExecutionBoundary1 opening_;
 friend Outcome<PendingCareHandoverStart> apply_care_handover_start_in_candidate(CommonStore&,OrderedTable<CareHandoverExecution>&,const CareHandoverStartContext&,CareHandoverStartOriginalWrite&,PreparedCareHandoverStart&&);
 friend Outcome<MomentOwnerSource> read_pending_care_handover_started_source(const PendingCareHandoverStart&,const CareHandoverStartContext&);
 friend Outcome<StableId> bind_care_handover_started_command_in_candidate(CommonStore&,OrderedTable<CareHandoverExecution>&,ActionElapsedStore&,const CareHandoverStartContext&,const CareHandoverBusinessRead&,PendingCareHandoverStart&,const StableId&,Integer,const CommitResult&);
public:
 PendingCareHandoverStart()=default;PendingCareHandoverStart(const PendingCareHandoverStart&)=delete;
 PendingCareHandoverStart& operator=(const PendingCareHandoverStart&)=delete;
 PendingCareHandoverStart(PendingCareHandoverStart&&)=default;PendingCareHandoverStart& operator=(PendingCareHandoverStart&&)=default;
 const StableId& action_id()const noexcept{return action_;}
};
// Pre-adoption only: reads this real just-applied source, without demanding the
// not-yet-created command result, Director adoption or FSL Started boundary.
Outcome<MomentOwnerSource> read_pending_care_handover_started_source(
 const PendingCareHandoverStart&,const CareHandoverStartContext&);
// Real Common result/history and any NPC Director adoption must already exist.
// No stable validation/publication may occur before this mandatory tail.
Outcome<StableId> bind_care_handover_started_command_in_candidate(
 CommonStore&,OrderedTable<CareHandoverExecution>&,ActionElapsedStore&,
 const CareHandoverStartContext&,const CareHandoverBusinessRead&,
 PendingCareHandoverStart&,const StableId& actual_actor,Integer request_seq,const CommitResult&);
}
