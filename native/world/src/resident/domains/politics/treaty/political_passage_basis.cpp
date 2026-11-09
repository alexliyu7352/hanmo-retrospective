#include "political_passage_decision.hpp"
namespace hanmo::resident {
// 比较已存在的请求目的值；不为决定补造尚未持久化的依据。
bool same_political_passage_purpose(const PassagePurposeContext &a,
                                    const PassagePurposeContext &b) {
  if (a.index() != b.index())
    return false;
  if (const auto *x = std::get_if<SafeExitPassageContext>(&a)) {
    const auto &y = std::get<SafeExitPassageContext>(b);
    return x->binding_id == y.binding_id && x->line_refs == y.line_refs &&
           x->path_id == y.path_id &&
           x->next_boundary_ordinal == y.next_boundary_ordinal;
  }
  return true;
}
// /1 原字段保持；/2 明确保留原目的、依据和半开窗口，不能以空向量冒普通依据。
Status validate_political_passage_authority_payload(
    const PoliticalPassageDecision &d) {
  if (d.source_schema_version == 1)
    return common_failure("PoliticalPassageOriginalBasisNotRecorded", {},
                          ErrorKind::unsupported_capability);
  if (d.source_schema_version != 2)
    return common_failure("PoliticalPassageSourceSchemaUnsupported");
  auto shape = validate_passage_purpose_context_shape(d.purpose_context);
  if (!shape.ok)
    return shape;
  if (d.requested_window.from_tick < 0 ||
      d.requested_window.until_tick <= d.requested_window.from_tick ||
      d.entry_tick != d.requested_window.from_tick)
    return common_failure("PoliticalPassageOriginalWindowMismatch");
  if (const auto *original =
          std::get_if<ExistingOriginalPassageBasis>(&d.authority_basis)) {
    if (original->policy_id.empty() || original->policy_version < 1 ||
        original->catalog_digest.empty())
      return common_failure("PoliticalPassageOriginalPolicyRequired");
    if (!std::holds_alternative<OrdinaryPassageContext>(d.purpose_context) ||
        !d.treaty_term_refs.empty())
      return common_failure("PoliticalPassageOriginalBasisMismatch");
  } else if (const auto *t =
                 std::get_if<ActivatedTreatyPassageBasis>(&d.authority_basis)) {
    if (!std::holds_alternative<OrdinaryPassageContext>(d.purpose_context) ||
        !valid_source_ref(t->activation_ref) || t->term_id.empty())
      return common_failure("PoliticalPassageTreatyBasisInvalid");
  } else {
    const auto &safe_exit =
        std::get<TreatySafeExitPassageBasis>(d.authority_basis);
    const auto *purpose =
        std::get_if<SafeExitPassageContext>(&d.purpose_context);
    if (!purpose || purpose->binding_id != safe_exit.binding_id ||
        safe_exit.policy_version < 1 ||
        !valid_source_ref(safe_exit.entry_receipt_ref))
      return common_failure("PoliticalPassageSafeExitBasisInvalid");
    for (const auto &ref : safe_exit.current_manifest_successor_refs)
      if (!valid_source_ref(ref))
        return common_failure("PoliticalPassageSafeExitBasisInvalid");
  }
  return common_success();
}
} // namespace hanmo::resident
