#pragma once
#include "dialogue_command.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/kernel/world_timeline.hpp"
#include "resident/kernel/source_ref.hpp"
#include <memory>
namespace hanmo::resident {
class CommonStore;class ProjectionWriter;
struct InteractionState;struct InteractionCatalog;struct ReceiptDetails;
struct ActorSocialOccupancyTransition;struct CommonCommandSourceView;
// This is an immutable appendix of the original Interaction owner, not an
// action/progress ledger. Its key is the actual outer Common result ID.
// The original opening supplies actor and effective request context. The saved
// command is the exact authored command confirmed at this exact action node.
struct InteractionActionConfirmationBuildView {
 const StableId& committed_result_id;const StableId& session_id;
 Integer session_generation;
 const StableId& dialogue_id;const StableId& action_node_id;
 const DialogueCommand& confirmed_command;const StableId& action_id;
 const WorldMoment& confirmation_at;
};
struct InteractionActionConfirmationSource {
 CheckpointCopyLease copied_fields; // first, runtime only
 InteractionActionConfirmationSource()=default;
 explicit InteractionActionConfirmationSource(const InteractionActionConfirmationBuildView&);
 InteractionActionConfirmationSource(const InteractionActionConfirmationSource&);
 InteractionActionConfirmationSource(InteractionActionConfirmationSource&&);
 InteractionActionConfirmationSource& operator=(const InteractionActionConfirmationSource&);
 InteractionActionConfirmationSource& operator=(InteractionActionConfirmationSource&&);
 void swap(InteractionActionConfirmationSource&);
 friend void swap(InteractionActionConfirmationSource&a,InteractionActionConfirmationSource&b){a.swap(b);}
 StableId committed_result_id,session_id;Integer session_generation=0;
 StableId dialogue_id,action_node_id;DialogueCommand confirmed_command;
 StableId action_id;WorldMoment confirmation_at;
};
// Only physical11/12/13 carries confirmation schema1. No inferred upgrade.
Integer interaction_action_confirmation_schema_version(Integer physical_version)noexcept;
Status validate_interaction_action_confirmation_shape(const InteractionActionConfirmationSource&);
class InteractionActionConfirmationPending final:public CheckpointAllocatedObject {
 friend Status prepare_interaction_action_confirmation_in_candidate(CommonStore&,InteractionState&,const InteractionCatalog&,const StableId&,const WorldMoment&,InteractionActionConfirmationPending&);
 friend Status record_interaction_action_confirmation_produced_in_candidate(CommonStore&,InteractionState&,const InteractionCatalog&,InteractionActionConfirmationPending&,const ReceiptDetails&);
 friend Status bind_interaction_action_confirmation_result_in_candidate(CommonStore&,InteractionState&,const InteractionCatalog&,InteractionActionConfirmationPending&,const SourceRef&);
 const CommonStore* common_;const InteractionState* state_;const InteractionCatalog* catalog_;
 std::optional<InteractionActionConfirmationSource> prepared_;
 Integer original_revision_=0;bool execution_recorded_=false,adopted_=false;
 InteractionActionConfirmationPending(CommonStore&c,InteractionState&s,const InteractionCatalog&cat):common_(&c),state_(&s),catalog_(&cat){}
public:
 static Outcome<std::unique_ptr<InteractionActionConfirmationPending>> create(CommonStore&,InteractionState&,const InteractionCatalog&);
 bool bound_to(const CommonStore&,const InteractionState&,const InteractionCatalog&)const noexcept;
 InteractionActionConfirmationPending(const InteractionActionConfirmationPending&)=delete;
 InteractionActionConfirmationPending& operator=(const InteractionActionConfirmationPending&)=delete;
 bool has_confirmation()const noexcept{return prepared_.has_value();}
};
// Prepare immediately before the one original nested apply, after its actual
// prepared command has been rechecked. The token is never saved in World.
Status prepare_interaction_action_confirmation_in_candidate(CommonStore&,InteractionState&,
 const InteractionCatalog&,const StableId& actor,const WorldMoment&,
 InteractionActionConfirmationPending&);
// Record that apply's actual receipt before finish_dialogue_confirmation can
// change/erase the session. A non-ActionStarted receipt creates no appendix.
Status record_interaction_action_confirmation_produced_in_candidate(CommonStore&,InteractionState&,
 const InteractionCatalog&,InteractionActionConfirmationPending&,const ReceiptDetails&);
// After the true outer seal/history and before Growth started-command binding.
// Failure aborts the same entire candidate. No future result ID is accepted.
Status bind_interaction_action_confirmation_result_in_candidate(CommonStore&,InteractionState&,
 const InteractionCatalog&,InteractionActionConfirmationPending&,const SourceRef& actual_outer_result);
struct OriginalInteractionActionConfirmation {
 const InteractionActionConfirmationSource* source=nullptr;
 const ActorSocialOccupancyTransition* opening=nullptr;
};
// The input is the real common/command_result, not a synthetic Interaction fact.
// Reads the exact result's typed ActionStarted and exact catalog/node/command.
Outcome<OriginalInteractionActionConfirmation> read_original_interaction_action_confirmation(
 const CommonStore&,const InteractionState&,const InteractionCatalog&,
 const SourceRef& actual_outer_result,const StableId& action_id);
Outcome<OriginalInteractionActionConfirmation> read_original_interaction_action_confirmation(
 const CommonStore&,const InteractionState&,const InteractionCatalog&,
 const StableId& actual_outer_result_id,const StableId& action_id);
enum class InteractionActionConfirmationPinRole {
 outer_result,session_opening,checked_session_close,
 checked_response,checked_response_commit,earlier_response_consumption
};
class InteractionActionConfirmationPinSink {
public:
 virtual ~InteractionActionConfirmationPinSink()=default;
 virtual Status original_source(InteractionActionConfirmationPinRole,
  const SourceRefBuildView&,Integer source_schema)=0;
};
// Same acyclic reader, with direct edges emitted at the checks which use them.
// The row must be the actual borrowed retained row, not a detached equal copy.
// The sink owns identity deduplication; this does not recurse into PIN owners.
Status visit_interaction_action_confirmation_pins(const CommonStore&,const InteractionState&,
 const InteractionCatalog&,const InteractionActionConfirmationSource&,
 InteractionActionConfirmationPinSink&);
Status validate_interaction_action_confirmation_sources(const CommonStore&,const InteractionState&,const InteractionCatalog&);
Status emit_interaction_action_confirmation_fields(ProjectionWriter&,const InteractionActionConfirmationSource&);
}
