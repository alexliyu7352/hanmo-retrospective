// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
#include "state_need_policy_read.hpp"
#include "resident/domains/actor_foundation/authored_origin.hpp"
namespace hanmo::resident {
namespace {
bool identity(const CatalogIdentity &a, const CatalogIdentity &b) {
  return a.content_id == b.content_id &&
         a.content_version == b.content_version &&
         a.content_digest == b.content_digest && a.ruleset_id == b.ruleset_id &&
         a.schema_sha256 == b.schema_sha256 &&
         a.pack_set_digest == b.pack_set_digest &&
         a.module_versions == b.module_versions &&
         a.artifact_sha256 == b.artifact_sha256;
}
Status care_policy(const StateNeedPolicySourceContext &x,
                   const StateNeedPolicy &p) {
  if (!x.care || !x.care_catalog)
    return common_failure("CareStateNeedPolicySourcesRequired", p.id,
                          ErrorKind::unsupported_capability);
  if (&x.care->common != &x.common || !x.care->rules.bound_to(x.common) ||
      !identity(x.care_catalog->identity, x.identity))
    return common_failure("CareStateNeedPolicyCandidateMismatch", p.id,
                          ErrorKind::internal);
  auto valid = validate_care_requirement_catalog(*x.care_catalog);
  if (!valid.ok)
    return valid;
  const auto *r = x.care_catalog->rules.find(p.owner_rule);
  if (!r || r->state_need_policy_ref != p.id ||
      r->disclosure_rule_ref.id != p.disclosure ||
      r->cancellation_rule_ref.id != p.cancellation_rule)
    return common_failure("CareStateNeedPolicyRuleMismatch", p.id,
                          ErrorKind::internal);
  auto actual = x.care->rules.read_care_requirement_rule(r->rule_ref);
  if (!actual.ok)
    return {false, {}, actual.error};
  if (actual.value != r)
    return common_failure("CareStateNeedPolicyCatalogBindingMismatch", p.id,
                          ErrorKind::internal);
  return common_success();
}
} // namespace
Status NativeStateNeedPolicyRead::validate_sources() const {
  const auto &x = context_;
  auto valid = validate_state_need_catalog(x.catalog);
  if (!valid.ok)
    return valid;
  if (x.identity.content_id.empty() || x.identity.content_version.empty() ||
      x.identity.content_digest.empty() || x.identity.ruleset_id.empty() ||
      x.identity.schema_sha256.empty())
    return common_failure("StateNeedLockedCatalogRequired", {},
                          ErrorKind::unsupported_capability);
  bool selected = false;
  for (const auto &v : x.identity.module_versions)
    if (v.first == "state_needs") {
      if (selected || (v.second != "1" && v.second != "2"))
        return common_failure("StateNeedPolicyModuleMismatch", {},
                              ErrorKind::internal);
      selected = true;
    }
  if (!selected)
    return common_failure("StateNeedPolicyModuleRequired", {},
                          ErrorKind::unsupported_capability);
  for (const auto &row : x.catalog.policies.rows()) {
    const auto &p = row.second;
    if (p.goal_kind == 0) {
      valid = care_policy(x, p);
      if (!valid.ok)
        return valid;
    } else {
      if (!x.medical)
        return common_failure("MedicalStateNeedPolicySourcesRequired", p.id,
                              ErrorKind::unsupported_capability);
      if (!x.medical->bound_to(x.common))
        return common_failure("MedicalStateNeedPolicyCandidateMismatch", p.id,
                              ErrorKind::internal);
      valid = x.medical->validate_medical_state_need_policy(p, x.identity);
      if (!valid.ok)
        return valid;
    }
    valid = x.tasks.validate_state_need_policy(x.common, x.identity, p);
    if (!valid.ok)
      return valid;
  }
  return common_success();
}
// 按目标的真实来源绑定政策；缺读口或规则不匹配时保留原拒绝。
Outcome<const StateNeedPolicy *>
NativeStateNeedPolicyRead::state_need_policy(const StateGoalRef &goal) const {
  auto valid = validate_sources();
  if (!valid.ok)
    return {false, nullptr, valid.error};
  const auto &x = context_;
  const StableId *id = nullptr;
  const CareRequirementRule *care_rule = nullptr;
  if (const auto *g = std::get_if<CareCoverageGoal>(&goal)) {
    if (!x.care)
      return {false, nullptr,
              common_failure("CareStateNeedPolicySourcesRequired",
                             g->care_requirement_id,
                             ErrorKind::unsupported_capability)
                  .error};
    auto actual = read_care_requirement_rule_for_goal(*x.care, *g);
    if (!actual.ok)
      return {false, nullptr, actual.error};
    if (!actual.value)
      return {false, nullptr,
              common_failure("CareStateNeedPolicyRuleMissing",
                             g->care_requirement_id, ErrorKind::internal)
                  .error};
    care_rule = actual.value;
    id = &actual.value->state_need_policy_ref;
  } else {
    const auto &medical_goal = std::get<MedicalResolutionGoal>(goal);
    if (!x.medical)
      return {false, nullptr,
              common_failure("MedicalStateNeedPolicySourcesRequired",
                             medical_goal.medical_need_id,
                             ErrorKind::unsupported_capability)
                  .error};
    auto actual = x.medical->read_medical_goal_policy_id(medical_goal);
    if (!actual.ok)
      return {false, nullptr, actual.error};
    id = actual.value;
  }
  const auto *p = id ? x.catalog.policies.find(*id) : nullptr;
  if (!p || p->id != *id || p->goal_kind != static_cast<Integer>(goal.index()))
    return {false, nullptr,
            common_failure("StateGoalPolicyBindingMismatch",
                           id ? *id : StableId{}, ErrorKind::internal)
                .error};
  if (care_rule && (!x.care_catalog ||
                    x.care_catalog->rules.find(p->owner_rule) != care_rule))
    return {false, nullptr,
            common_failure("CareStateGoalOwnerRuleMismatch", p->id,
                           ErrorKind::internal)
                .error};
  return {true, p, {}};
}
} // namespace hanmo::resident
