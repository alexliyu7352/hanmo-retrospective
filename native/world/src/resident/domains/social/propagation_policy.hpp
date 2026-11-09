#pragma once
#include "knowledge_category.hpp"
#include "resident/domains/interaction/communication_delivery_read.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
// N§6.0 唯一作者条款，数值由内容明确给出，不增人格或自由评分脚本。
struct PropagationPolicy {
 StableId policy_id,rule_version;
 std::vector<KnowledgeCategory> enabled_categories;
 std::vector<MessageChannelKind> allowed_channels;
 Integer max_candidates=0,max_dispatches_per_review=0;
 struct Thresholds {Integer cooperative=0,selfish=0;} relation_thresholds_by_social_kind;
 Integer expiry_ticks=0,cooldown_ticks=0;
};
struct PropagationPolicyLimits {Integer max_expiry_ticks=0,max_cooldown_ticks=0;};
const char* propagation_category_name(KnowledgeCategory)noexcept;
Outcome<KnowledgeCategory> read_propagation_category(const StableId&);
Status validate_propagation_policy(const PropagationPolicy&,const PropagationPolicyLimits&);
Outcome<PropagationPolicy> decode_propagation_policy(const BoundaryValue&,const PropagationPolicyLimits&);
Status emit_propagation_policy_fields(ProjectionWriter&,const PropagationPolicy&);
}
