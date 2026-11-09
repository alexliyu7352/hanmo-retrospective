#include "political_foundation_decisions.hpp"

namespace hanmo::resident {
// 只校来源的有限物理形状，真实控制/能力/获知由原完整 reader 回核。
Status
validate_foundation_intent_source_shape(const FoundationIntentSource &row) {
  if (row.id.empty() || row.terms_revision != 1)
    return common_failure("InvalidFoundationIntentSource", row.id);
  auto valid = validate_foundation_intent_terms_shape(row.terms);
  if (!valid.ok)
    return valid;
  auto at = encode_world_moment(row.proposed_at);
  return at.ok ? common_success() : Status{false, {}, at.error};
}
Status
validate_foundation_support_source_shape(const FoundationSupportSource &row) {
  if (row.id.empty() || row.terms_revision != 1 || row.actor_id.empty() ||
      !valid_source_ref(row.intent_source) ||
      row.intent_source.domain != "politics" ||
      row.intent_source.kind != foundation_intent_kind ||
      (row.decision != FoundationSupportDecision::accept &&
       row.decision != FoundationSupportDecision::refuse) ||
      !valid_source_ref(row.actual_content_receipt.source_ref) ||
      row.actual_content_receipt.source_ref.domain != "interaction" ||
      row.actual_content_receipt.source_ref.kind != "communication_delivery" ||
      row.actual_content_receipt.source_schema != 1)
    return common_failure("InvalidFoundationSupportSource", row.id);
  auto at = encode_world_moment(row.decided_at);
  return at.ok ? common_success() : Status{false, {}, at.error};
}
} // namespace hanmo::resident
