// Explicit original Social physical branch; dynamic_social selection is separate.
#pragma once
#include "social_model.hpp"
namespace hanmo::resident {
class ProjectionWriter;
Status validate_social_relation_state_selection(const SocialState&,Integer selected_version);
Outcome<SocialState> decode_social_seed(const BoundaryValue&,Integer selected_version);
BoundaryValue encode_social_state(const SocialState&,Integer selected_version);
Status emit_social_state_fields(ProjectionWriter&,const SocialState&,Integer selected_version);
Status visit_social_checkpoint(CodecVisitor&,SocialState&,Integer selected_version);
}
