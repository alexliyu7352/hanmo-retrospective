// P24.1: the actual caregiver's StopCare command. Its one result payload is
// CareFamilyActivity.ended_by.cause; no second stop or command history table.
#pragma once
#include "care_family_activity_read.hpp"
#include "resident/kernel/read_stamp.hpp"
namespace hanmo::resident {
struct CommitResult;
SourceRef care_personal_stop_source(const StableId&);
Outcome<Integer> care_personal_stop_source_schema(const SourceRef&);
Status validate_care_personal_stop_shape(const CareFamilyActivity&,const CarePersonalStop&);
bool care_personal_stop_equal(const CarePersonalStop&,const CarePersonalStop&);
struct CarePersonalStopSourceRead {
 const CareFamilyActivity* activity=nullptr;
 const CareFamilyActivityEnd* end=nullptr;
 const CarePersonalStop* stop=nullptr;
 const InitialConditionSeed* actual_condition_seed=nullptr;
 const ConditionReceiptV2* actual_condition_before=nullptr;
};
// Source-local original start, true result/history, own command route and
// actual Common order; deliberately does not call Lost/Need or full activity.
Outcome<CarePersonalStopSourceRead> read_care_personal_stop_source(
 const CareFamilyActivitySources&,const MomentOwnerSource&);
struct StopCareRequest {StableId activity_id;};
struct CarePersonalStopContext {
 const CareFamilyActivitySources& sources;const CurrentReadStampRead& reads;
 const ReadStampAuthority& stamps;const DomainReadVersionSource& versions;
};
class PreparedCarePersonalStop;class PendingCarePersonalStop;
Outcome<PreparedCarePersonalStop> prepare_care_personal_stop(
 const CarePersonalStopContext&,const StableId& actor,const StopCareRequest&);
Outcome<PendingCarePersonalStop> apply_care_personal_stop_in_candidate(
 CommonStore&,const CarePersonalStopContext&,PreparedCarePersonalStop&&);
class PreparedCarePersonalStop {
 const CommonStore* common_=nullptr;const CareFamilyActivitySources* sources_=nullptr;
 StableId actor_,activity_,binding_,goal_,transition_;WorldMoment at_;
 ActorConditionHead condition_;std::shared_ptr<const ReadStamp> stamp_;
 friend Outcome<PreparedCarePersonalStop> prepare_care_personal_stop(const CarePersonalStopContext&,const StableId&,const StopCareRequest&);
 friend Outcome<PendingCarePersonalStop> apply_care_personal_stop_in_candidate(CommonStore&,const CarePersonalStopContext&,PreparedCarePersonalStop&&);
public:
 PreparedCarePersonalStop()=default;PreparedCarePersonalStop(const PreparedCarePersonalStop&)=delete;
 PreparedCarePersonalStop& operator=(const PreparedCarePersonalStop&)=delete;
 PreparedCarePersonalStop(PreparedCarePersonalStop&&)=default;PreparedCarePersonalStop& operator=(PreparedCarePersonalStop&&)=default;
};
class PendingCarePersonalStop {
 const CommonStore* common_=nullptr;const CareFamilyActivitySources* sources_=nullptr;
 StableId actor_,activity_,binding_,goal_,transition_,stop_;WorldMoment at_;ActorConditionHead condition_;
 friend Outcome<PendingCarePersonalStop> apply_care_personal_stop_in_candidate(CommonStore&,const CarePersonalStopContext&,PreparedCarePersonalStop&&);
 friend Outcome<MomentOwnerSource> read_pending_care_personal_stop_source(const PendingCarePersonalStop&,const CarePersonalStopContext&);
 friend Outcome<BirthCareNeedApplication> bind_care_personal_stop_command_in_candidate(CommonStore&,CareGoalWriteTables,CareCoverageWriteTables,CareFamilyActivityWriteBinding&,const CarePersonalStopContext&,PendingCarePersonalStop&,const StableId&,Integer,const CommitResult&,AffairsState&,const StateNeedPolicyRead&,const StateGoalRead&,StateTaskConsumer&);
public:
 PendingCarePersonalStop()=default;PendingCarePersonalStop(const PendingCarePersonalStop&)=delete;
 PendingCarePersonalStop& operator=(const PendingCarePersonalStop&)=delete;
 PendingCarePersonalStop(PendingCarePersonalStop&&)=default;PendingCarePersonalStop& operator=(PendingCarePersonalStop&&)=default;
 const StableId& actual_stop_id()const noexcept{return stop_;}
};
// Only this non-decoded, same-candidate Pending can expose the real header
// before the command is sealed. Ordinary stable readers never accept it.
Outcome<MomentOwnerSource> read_pending_care_personal_stop_source(
 const PendingCarePersonalStop&,const CarePersonalStopContext&);
// After actual Common cache/history, and NPC adoption when applicable:
// bind command -> actual activity end -> Lost -> this goal's original Need.
// No whole-world publication is permitted between apply and this tail.
Outcome<BirthCareNeedApplication> bind_care_personal_stop_command_in_candidate(
 CommonStore&,CareGoalWriteTables,CareCoverageWriteTables,CareFamilyActivityWriteBinding&,
 const CarePersonalStopContext&,PendingCarePersonalStop&,const StableId& actor,Integer request_seq,
 const CommitResult&,AffairsState&,const StateNeedPolicyRead&,const StateGoalRead&,StateTaskConsumer&);
struct CarePersonalStopCommandReference {
 const CareFamilyActivity* activity=nullptr;const CarePersonalStop* stop=nullptr;
};
class CarePersonalStopCommandReferenceSink {
public:virtual ~CarePersonalStopCommandReferenceSink()=default;
 virtual Status actual_reference(const CarePersonalStopCommandReference&)=0;
};
Status visit_care_personal_stop_command_result_references(
 const CareFamilyActivitySources&,CarePersonalStopCommandReferenceSink&);
}
