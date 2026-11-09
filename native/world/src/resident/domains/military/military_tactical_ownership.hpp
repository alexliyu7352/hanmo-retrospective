#pragma once
#include "military_tactical_ownership_model.hpp"
#include "military_tactical_intent_read.hpp"
#include "military_formation_source.hpp"
#include "resident/kernel/source_retention.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
namespace hanmo::resident {
// Non-owning construction inputs for the original K9 raw planner. They have
// exactly the persisted members, not an alternate data model or field walker.
// All referenced source/output buffers must already have their own lifetime
// charge before these views are formed. Planning precedes the first target copy.
struct MilitaryTacticalFormationBirthBuildView3 {
 const MomentOwnerSource& original_admission_source;const WorldMoment& created_at;
 const CommonActualFactPrefix& actual_before_common_prefix;
 const std::vector<MomentOwnerSource>& original_source_refs;
};
struct MilitaryTacticalFormationBuildView3 {
 const StableId& formation_id;const StableId& source_army_ref;const Integer& roster_revision;
 const std::optional<StableId>& naval_carrier_ref;const MilitaryFormationSpatial3& spatial_binding;
 const std::vector<StableId>& member_unit_refs;const std::optional<StableId>& officer_assignment_ref;
 const std::optional<StableId>& superior_formation_ref;const StableId& mission_terms_ref;
 const MilitaryFormationControl3& control;const std::optional<StableId>& active_intent_ref;
 const Integer& last_applied_order_ordinal;const Integer& control_revision;
 const Integer& command_binding_revision;const Integer& intent_revision;
 const MilitaryTacticalFormationBirthBuildView3& birth;
};
struct MilitaryPlayerDirectIntentBuildView3 {
 const StableId& controller_actor_id;const Integer& control_epoch;
 const MomentOwnerSource& actual_command_source;
};
struct MilitaryAcceptedDirectIntentBuildView3 {
 const StableId& id;const StableId& session_ref;const Integer& generation;
 const StableId& formation_ref;const StableId& mission_ref;const StableId& unit_ref;
 const WorldMoment& accepted_at;const Integer& action_instance_ordinal;
 const MilitaryOrder& original_order;const StableId& timing_definition_ref;
 const Integer& timing_definition_version;const MomentOwnerSource& control_binding_source;
 const MomentOwnerSource& mission_scope_source;const MilitaryPlayerDirectIntentBuildView3& adoption;
};
struct MilitaryTacticalMissionBuildView3 {
 const StableId& id;const StableId& formation_ref;const Integer& intent_revision;
 const WorldMoment& adopted_at;const std::optional<StableId>& previous_intent_ref;
 const DirectForceIntent3& original_direct_request;
 const CommonActualFactPrefix& actual_before_common_prefix;
 const MomentOwnerSource& control_binding_source;
 const StableId& original_solver_definition_ref;const Integer& original_solver_definition_version;
 const std::vector<MomentOwnerSource>& original_scope_sources;
 const std::vector<MilitaryMissionOriginalExecution3>& original_executions;
 const std::vector<MilitaryAcceptedIntent3>& accepted_intents;
 const std::optional<MomentOwnerSource>& actual_command_source;
};
struct DirectForceIntentReceiptBuildView3 {
 const StableId& formation_id;const StableId& mission_ref;
 const Integer& previous_intent_revision;const Integer& intent_revision;
 const WorldMoment& accepted_at;const std::vector<SourceRef>& accepted_intent_refs;
};
// The selected original admission provider supplies the finite original
// Army/Unit/control/spatial/upper-terms binding. It is not a generic setter.
struct MilitaryTacticalFormationAdmission3 {
 StableId source_army_ref;Integer roster_revision=0;
 std::optional<StableId> naval_carrier_ref;MilitaryFormationSpatial3 spatial_binding;
 std::vector<StableId> member_unit_refs;std::optional<StableId> officer_assignment_ref;
 std::optional<StableId> superior_formation_ref;StableId mission_terms_ref;
 MilitaryFormationControl3 control;Integer control_revision=0,command_binding_revision=0;
 WorldMoment admitted_at;std::vector<MomentOwnerSource> original_source_refs;
};
struct MilitaryTacticalActionPlan3 {
 StableId unit_ref;MilitaryOrder original_order;
 StableId timing_definition_ref;Integer timing_definition_version=0;
};
// Produced by the real selected mission solver, not by the caller or owner
// copying a UI action. Every member has exactly one action OR original execution.
struct MilitaryDirectMissionInstallation3 {
 StableId solver_definition_ref;Integer solver_definition_version=0;
 std::vector<MomentOwnerSource> scope_sources;
 std::vector<MilitaryTacticalActionPlan3> actions;
 std::vector<MilitaryMissionOriginalExecution3> original_executions;
};
class MilitaryTacticalOriginalSources3 {
public:
 virtual ~MilitaryTacticalOriginalSources3()=default;
 virtual bool bound_to(const CommonStore&,const MilitaryState&,const MilitaryExactState&)const noexcept=0;
 virtual Outcome<MilitaryTacticalFormationAdmission3> read_actual_formation_admission(const MomentOwnerSource&)const=0;
 // Historical source-local validation at the immutable pre-acceptance cut.
 // Must not call whole tactical/session/action validation recursively.
 virtual Status validate_formation_admission_before(const MilitaryTacticalFormation3&)const=0;
 virtual Status validate_direct_permission_now(const StableId& actor,const DirectForceIntent3&,const MilitaryTacticalFormation3&)const=0;
 virtual Status validate_original_mission_scope(const MilitaryTacticalMissionRecord3&,const MilitaryTacticalFormation3&)const=0;
 virtual Status validate_original_mission_execution(const MilitaryTacticalMissionRecord3&,const MilitaryTacticalFormation3&)const=0;
 // Original sealed ReceiptDetails must be the exact DirectForceIntent3 result.
 // A different successful legacy command or actor/tick-only match must fail.
 virtual Status validate_direct_command_seal(const CommonCommandSourceView&,const DirectForceIntent3&,const DirectForceIntentReceipt3&)const=0;
 virtual Status validate_original_decision_inputs(const DynamicSessionStateSource2&)const=0;
 virtual Status validate_original_delivered_intent(const MilitaryAcceptedIntent3&)const=0;
};
// Mutating original mission execution installer. It must call the selected
// real S1/S2/naval solver, initialization, cost and handoff owners. Returning a
// source ref without that actual original writer is not an implementation.
// The enclosing complete World candidate owns rollback, not a Common savepoint.
class MilitaryTacticalMissionInstaller3 {
public:
 virtual ~MilitaryTacticalMissionInstaller3()=default;
 virtual bool bound_to(const CommonStore&,const MilitaryState&,const MilitaryExactState&)const noexcept=0;
 virtual Outcome<MilitaryDirectMissionInstallation3> install_direct_mission_in_candidate(
     const StableId& mission_ref,const DirectForceIntent3&,const MilitaryTacticalFormation3&,
     const WorldMoment&,const CommonActualFactPrefix&)=0;
};
struct MilitaryTacticalOwnershipContext3 {
 const MilitaryTacticalOriginalSources3& originals;const CurrentMomentRead& clock;
 const MilitaryFormationSourceRead& army_formations;
 ExactArithmeticBudget budget={};
};
// Concrete source provider owns no values and always returns the actual
// nested mission row. Reconstruct after World clone/move/restore.
class NativeMilitaryTacticalIntentRead3 final:public MilitaryTacticalIntentOriginalRead {
 const CommonStore& common_;const MilitaryState& military_;const MilitaryExactState& exact_;
 MilitaryTacticalOwnershipContext3 context_;
public:
 NativeMilitaryTacticalIntentRead3(const CommonStore&,const MilitaryState&,const MilitaryExactState&,MilitaryTacticalOwnershipContext3);
 Integer selected_intent_schema()const noexcept override{return 3;}
 bool bound_to(const CommonStore&,const MilitaryExactState&)const noexcept override;
 Outcome<const MilitaryAcceptedIntent3*> read_original_accepted_intent(const SourceRef&)const override;
 Status visit_original_accepted_intents(MilitaryAcceptedIntentVisitor&)const override;
 Status validate_original_intent_scope(const MilitaryAcceptedIntent3&)const override;
 Status validate_original_direct_intent_command(const MilitaryAcceptedIntent3&,const CommonCommandSourceView&)const override;
 Status validate_original_decision_inputs(const DynamicSessionStateSource2&)const override;
 Status validate_original_delivered_intent(const MilitaryAcceptedIntent3&)const override;
};
MomentOwnerSource military_tactical_formation_source3(const StableId&);
MomentOwnerSource military_tactical_mission_source3(const StableId&);
// Exact typed comparison for the original Common ReceiptData alternative.
// No JSON/projection encoding, no synthesized result and no borrowed tick-only
// equality. The central command-source adapter owns the variant selection.
Outcome<bool> same_direct_force_intent_receipt3(
 const DirectForceIntentReceipt3&,const DirectForceIntentReceipt3&,ExactArithmeticBudget={});
Outcome<StableId> adopt_original_tactical_formation_in_candidate(
 CommonStore&,const MilitaryState&,MilitaryExactState&,const MilitaryTacticalOwnershipContext3&,
 const MomentOwnerSource& actual_original_admission);
// Zero-time prepare allocates no identity, consumes no resource/RNG and starts
// no work. Its owned request/prefix copies use the original command-copy budget.
class ProducedDirectForceIntent3;
class PreparedDirectForceIntent3 {
 friend Outcome<PreparedDirectForceIntent3> prepare_direct_force_intent3(const CommonStore&,const MilitaryState&,const MilitaryExactState&,const MilitaryTacticalOwnershipContext3&,const StableId&,const DirectForceIntent3&);
 friend Outcome<ProducedDirectForceIntent3> apply_direct_force_intent3_in_candidate(CommonStore&,const MilitaryState&,MilitaryExactState&,const MilitaryTacticalOwnershipContext3&,MilitaryTacticalMissionInstaller3&,const PreparedDirectForceIntent3&);
 // First member: released after all ordinary owned fields. This transient
 // charge never credits the separate persisted mission/receipt copies.
 CheckpointMemoryCharge copied_fields_;
 const CommonStore* common_=nullptr;const MilitaryState* military_=nullptr;const MilitaryExactState* exact_=nullptr;
 StableId actor_;DirectForceIntent3 request_;CommonActualFactPrefix prefix_;
public:
 PreparedDirectForceIntent3()=default;
 PreparedDirectForceIntent3(const PreparedDirectForceIntent3&)=default;
 // WorldMoment's value transfer can throw. Fresh copy retains the source's
 // original charge and reserves the destination before copying any field.
 PreparedDirectForceIntent3(PreparedDirectForceIntent3&& other)
  :PreparedDirectForceIntent3(static_cast<const PreparedDirectForceIntent3&>(other)){}
 PreparedDirectForceIntent3& operator=(const PreparedDirectForceIntent3&)=delete;
 PreparedDirectForceIntent3& operator=(PreparedDirectForceIntent3&&)=delete;
};
Outcome<PreparedDirectForceIntent3> prepare_direct_force_intent3(
 const CommonStore&,const MilitaryState&,const MilitaryExactState&,const MilitaryTacticalOwnershipContext3&,
 const StableId& authenticated_actor,const DirectForceIntent3&);
// The original command receipt leaves the domain under its own transient
// lifetime charge. Central copies receipt() into its immutable ReceiptDetails
// under that original owner's separate reservation; it must not steal this lease.
class ProducedDirectForceIntent3 {
 friend Outcome<ProducedDirectForceIntent3> apply_direct_force_intent3_in_candidate(CommonStore&,const MilitaryState&,MilitaryExactState&,const MilitaryTacticalOwnershipContext3&,MilitaryTacticalMissionInstaller3&,const PreparedDirectForceIntent3&);
 CheckpointMemoryCharge copied_fields_;DirectForceIntentReceipt3 receipt_;
public:
 ProducedDirectForceIntent3()=default;
 ProducedDirectForceIntent3(const ProducedDirectForceIntent3&)=default;
 ProducedDirectForceIntent3(ProducedDirectForceIntent3&& other)
  :ProducedDirectForceIntent3(static_cast<const ProducedDirectForceIntent3&>(other)){}
 ProducedDirectForceIntent3& operator=(const ProducedDirectForceIntent3&)=delete;
 ProducedDirectForceIntent3& operator=(ProducedDirectForceIntent3&&)=delete;
 const DirectForceIntentReceipt3& receipt()const noexcept{return receipt_;}
};
Outcome<ProducedDirectForceIntent3> apply_direct_force_intent3_in_candidate(
 CommonStore&,const MilitaryState&,MilitaryExactState&,const MilitaryTacticalOwnershipContext3&,
 MilitaryTacticalMissionInstaller3&,const PreparedDirectForceIntent3&);
// Required immediately after the original successful Common seal. Receipt
// already names real allocated mission/accepted IDs before its immutable copy.
// This function binds the real seal, then starts original positive-time actions.
// It never seals, publishes, advances time or applies attacks. Its expected
// receipt comparison uses a separately charged transient copy.
Status bind_direct_force_intent3_command_source_in_candidate(
 CommonStore&,const MilitaryState&,MilitaryExactState&,const MilitaryTacticalOwnershipContext3&,
 const DynamicMilitaryActionContext&,const DirectForceIntentReceipt3&,const MomentOwnerSource& actual_command_source);
Status validate_military_tactical_ownership3(
 const CommonStore&,const MilitaryState&,const MilitaryExactState&,const MilitaryTacticalOwnershipContext3&);
Outcome<WorldMoment> read_military_tactical_ownership_source_moment3(
 const CommonStore&,const MilitaryState&,const MilitaryExactState&,const MilitaryTacticalOwnershipContext3&,
 const MomentOwnerSource&);
Status collect_military_tactical_ownership_pins3(
 const CommonStore&,const MilitaryState&,const MilitaryExactState&,const MilitaryTacticalOwnershipContext3&,SourcePinSink&);
// Finite compiled consumer slots. Central pairs each with the selected original
// source-kind bounds and this domain's actual enumeration/validation.
std::vector<RequiredSourceConsumerSlot> military_tactical_ownership_consumer_slots3();
Status validate_military_tactical_ownership_pin3(
 const CommonStore&,const MilitaryState&,const MilitaryExactState&,const MilitaryTacticalOwnershipContext3&,const SourcePin&);
// These immutable rows have no independent pruning writer: their true retained
// original payload must remain while the enclosing /3 owner still contains it.
// A missing/mismatched source is an error, not permission to release.
Outcome<bool> military_tactical_ownership_source_may_release3(
 const CommonStore&,const MilitaryState&,const MilitaryExactState&,const MilitaryTacticalOwnershipContext3&,const MomentOwnerSource&);
Outcome<std::uint64_t> read_military_tactical_domain_revision3(
 const CommonStore&,const MilitaryState&,const MilitaryExactState&,const MilitaryTacticalOwnershipContext3&,const DomainReadKey&);
}
