// Original Task actions own their accepted terms and applied end result; S0 owns all credited work.
#pragma once
#include "task_model.hpp"
#include "resident/kernel/action_time_validation.hpp"
#include "resident/kernel/exact_phase_graph.hpp"
#include "resident/kernel/source_retention.hpp"
#include <array>
#include <string_view>
#include "resident/domains/family/family_life_exact.hpp"
namespace hanmo::resident {
struct TaskExactLifeSources;
Status validate_task_claim_acceptance_execution(const CommonStore&,const TaskState&,const TaskClaimEstablishedSource&,const TaskExactLifeSources* life=nullptr);
// Called only from the original ordinary-record branch after its existing
// Work/StateNeed/Shop/Governance/Envoy/contract owner dispatch.
Status validate_task_exact_ordinary_acceptance_record(const CommonStore&,const TaskState&,
 const TaskRecord&,const TaskExactLifeSources* life=nullptr);
struct TaskExactLifeSources {
 const CommonStore& common;const FamilyDeathState& deaths;
 const FamilyLifeCauseSources4& causes;const WorldMoment& actual_now;
};
bool task_exact_execution_version(Integer)noexcept;
Integer task_exact_terminal_source_schema(Integer selected_version)noexcept;
// Explicit fresh selection only, never reconstruction from retained legacy
// actions. Header identity is a bootstrap gate, not the complete Life/S0 proof.
Outcome<TaskExactExecutionState> make_initial_task_exact_execution(Integer selected_version);
Status validate_task_exact_execution_headers(const CommonStore&,const TaskState&,Integer selected_version);
bool owns_task_exact_original_fact_kind(const StableId&)noexcept;
// Only an actual borrowed Common header can identify the original payload.
// Zero denotes another fact family, not a fabricated exact source.
Outcome<Integer> read_task_exact_original_fact_schema(const CommonStore&,const TaskState&,const FactRecord&,Integer selected_version);
Status guard_task_exact_release(const CommonStore&,const TaskState&,const ActionElapsedStore&,const StableId&,const TaskExactLifeSources* life=nullptr);
bool exact_task_action_kind(const StableId&) noexcept;
std::vector<ActionTimeRegistration> task_action_time_registrations(Integer selected_version=1);
class TaskActionTimeSourceRead final:public ActionTimeOriginalSourceRead {
 const CommonStore& common_;const TaskState& tasks_;const TaskCatalog& catalog_;const TimeScaleRef& scale_;const CatalogIdentity& identity_;ExactArithmeticBudget budget_;const TaskExactLifeSources* life_;
public:
 TaskActionTimeSourceRead(const CommonStore&c,const TaskState&t,const TaskCatalog&catalog,const TimeScaleRef&scale,const CatalogIdentity&identity,ExactArithmeticBudget budget={},const TaskExactLifeSources* life=nullptr):common_(c),tasks_(t),catalog_(catalog),scale_(scale),identity_(identity),budget_(budget),life_(life){}
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
Status record_exact_task_started_in_candidate(CommonStore&,TaskState&,const TaskCatalog&,const ActionCoreView&,const TaskQuote&,TaskExactWrite&);
Status record_exact_task_terminal_in_candidate(CommonStore&,TaskState&,const TaskCatalog&,const ActionCoreView&,const TaskEffect&,std::size_t first_new_fact,TaskExactWrite&);
Outcome<WorldMoment> read_task_exact_source_moment(const CommonStore&,const TaskState&,const SourceRef&,Integer schema,const TaskExactLifeSources* life=nullptr);
// Original I19.1 stop. It does not finish, settle, pay, transfer, change Task
// status/deadline/evidence, or select a successor. The caller then uses the sole
// original release_action on the returned action before SessionLife reads it.
Outcome<const TaskExactTerminalSource*> stop_exact_task_for_death_in_candidate(
 CommonStore&,TaskState&,const TaskCatalog&,const StableId& action,const DeathRef&,
 TaskExactWrite&,const TaskExactLifeSources&);
Outcome<const TaskExactTerminalSource*> read_original_task_death_terminal(
 const CommonStore&,const TaskState&,const TaskCatalog&,const CatalogIdentity&,
 const StableId& action,const DeathRef&,const TaskExactLifeSources&);
// Borrow the original successful non-contract report branch only. This is the
// existing +4/+1 relation cause, not a rule for dynamic/quantity/objective Work.
// Source-local: no Social/Knowledge read and no future Common command result.
struct OriginalSuccessfulTaskReport {
 const TaskExactStartSource* started=nullptr;
 const TaskExactTerminalSource* completed=nullptr;
 const TaskRecord* task=nullptr;
};
Outcome<OriginalSuccessfulTaskReport> read_original_successful_task_report(
 const CommonStore&,const TaskState&,const TaskCatalog&,const CatalogIdentity&,
 const SourceRef& actual_terminal,Integer source_schema,const WorldMoment& actual_now);
Status validate_task_exact_execution(const CommonStore&,const TaskState&,const TaskCatalog&,const ActionElapsedStore&,const WorldMoment&,const TimeScaleRef&,const CatalogIdentity&,const TaskExactLifeSources* life=nullptr);
// Direct original-row edges only. The global registry owns source eligibility
// and the one charged collector; this visitor neither recurses nor retains an
// all-world pin vector. Explicit selected_version is not inferred from rows.
struct TaskExactSourcePinSlotDeclaration {
 std::string_view consumer_kind,slot;
 bool multiple_sources;
};
inline constexpr std::array<TaskExactSourcePinSlotDeclaration,5> task_exact_source_pin_slots{{
 {"task_action_started","original_start",true},
 {"task_action_started","terms_revision",false},
 {"task_action_finished","start",false},
 {"task_action_finished","actual_result",true},
 {"task_action_finished","participant_death",false}
}};
Status visit_task_exact_original_source_pins(const CommonStore&,const TaskState&,const TaskCatalog&,
 const CatalogIdentity&,Integer selected_version,SourcePinSink&,const TaskExactLifeSources* life=nullptr);
Status validate_task_exact_original_source_pin(const CommonStore&,const TaskState&,const TaskCatalog&,
 const CatalogIdentity&,Integer selected_version,const SourcePin&,const TaskExactLifeSources* life=nullptr);

// One concrete WorldBundle registration, actual Task writer and original service adapters.
struct TaskWorkWorldFactory;
ExactPhaseRegistration make_task_exact_phase_registration();
ExactPhaseRegistration make_task_exact_phase_registration(TaskWorkWorldFactory,Integer selected_version=1);
Outcome<TaskExactExecutionState> decode_task_exact_execution(const BoundaryValue&);
Outcome<TaskExactExecutionState> decode_task_exact_execution(const BoundaryValue&,Integer selected_version);
Outcome<BoundaryValue> encode_task_exact_execution(const TaskExactExecutionState&);
Outcome<BoundaryValue> encode_task_exact_execution(const TaskExactExecutionState&,Integer selected_version);
Status visit_task_exact_execution_checkpoint(CodecVisitor&,TaskExactExecutionState&);
Status visit_task_exact_execution_checkpoint(CodecVisitor&,TaskExactExecutionState&,Integer selected_version);
class ProjectionWriter;
Outcome<bool> emit_task_exact_original_fact_payload_fields(ProjectionWriter&,const CommonStore&,const TaskState&,const FactRecord&,Integer selected_version);
Status emit_task_exact_execution_projection(ProjectionWriter&,std::string_view,const TaskExactExecutionState&);
Status emit_task_exact_execution_projection(ProjectionWriter&,std::string_view,const TaskExactExecutionState&,Integer selected_version);
}
