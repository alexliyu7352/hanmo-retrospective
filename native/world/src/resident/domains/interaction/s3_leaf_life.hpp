#include "resident/kernel/source_ref.hpp"
// Fixed S3 register v03 §4.0. Values embedded in each original owner's life_sources.
// There is deliberately no common mutable lifecycle state or shared lifecycle writer.
#pragma once
#include "resident/domains/family/death_ref.hpp"
#include "resident/domains/actor_foundation/actor_foundation_refs.hpp"
namespace hanmo::resident {
struct S3SessionStopped {std::optional<SourceRef> original_action_terminal_ref,selected_response_tail_ref;};
struct S3InquiryInterrupted {SourceRef original_action_terminal_ref;};
struct S3AppointmentParticipationStopped {std::optional<SourceRef> start_fact_ref,original_action_terminal_ref,actual_meeting_end_ref;};
enum class S3MessageLifeDisposition {held,returned,awaiting_lawful_successor};
struct S3MessageResponsibilityContinued {S3MessageLifeDisposition disposition=S3MessageLifeDisposition::held;SourceRef actual_channel_result_ref;std::optional<SourceRef> successor_authority_ref;};
enum class S3TaskLifeDisposition {execution_stopped,awaiting_authorized_successor,continued_under_original_terms};
struct S3TaskResponsibilityContinued {S3TaskLifeDisposition disposition=S3TaskLifeDisposition::execution_stopped;std::optional<SourceRef> original_action_terminal_ref,original_task_terminal_ref,actual_authority_ref;std::vector<SourceRef> remaining_responsibility_refs;};
using S3LeafLifeEffectValue=std::variant<S3SessionStopped,S3InquiryInterrupted,S3AppointmentParticipationStopped,S3MessageResponsibilityContinued,S3TaskResponsibilityContinued>;
struct S3LeafLifeEffect {StableId result_id;std::optional<SourceRef> original_terms_ref;S3LeafLifeEffectValue effect;std::vector<SourceRef> preserved_result_refs;ActorFoundationAt effective_game_time;};
Status validate_s3_leaf_life_effect_shape(const S3LeafLifeEffect&,std::size_t expected_effect,bool legacy_time_allowed=false);
}
