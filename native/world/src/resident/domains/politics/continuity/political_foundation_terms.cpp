#include "political_foundation_terms.hpp"

namespace hanmo::resident {
namespace {
// 全身份一致才可借原作者要求；不能仅凭相同摘要拼入另一场景目录。
bool same_catalog(const CatalogIdentity &a, const CatalogIdentity &b) {
  return a.content_id == b.content_id &&
         a.content_version == b.content_version &&
         a.artifact_sha256 == b.artifact_sha256 &&
         a.content_digest == b.content_digest && a.ruleset_id == b.ruleset_id &&
         a.schema_sha256 == b.schema_sha256 &&
         a.pack_set_digest == b.pack_set_digest &&
         a.module_versions == b.module_versions;
}

// 仅拒一个原角色中的坏键/重复键；同一实际结果可合法证明不同角色，
// 跨角色关系须由各原 owner 检查，不能在这里增设全表唯一要求。
Status validate_sources(const std::vector<SourceRef> &refs) {
  for (std::size_t i = 0; i < refs.size(); ++i) {
    if (!valid_source_ref(refs[i]))
      return common_failure("InvalidFoundationSource", refs[i].id);
    for (std::size_t j = 0; j < i; ++j)
      if (same_source_ref(refs[i], refs[j]))
        return common_failure("DuplicateFoundationSource", refs[i].id);
  }
  return common_success();
}
// 两种表示共用原输入规则，避免最终条款和意向的准入条件漂移。
template <class Terms> Status validate_inputs(const Terms &terms) {
  if (!foundation_kind_tag(terms.kind))
    return common_failure("UnknownFoundationKind");
  if (terms.organization_kind.empty() || terms.constitution_id.empty() ||
      terms.initial_office_id.empty() ||
      terms.initial_grant_template_id.empty() || terms.founder.empty())
    return common_failure("FoundationTermsIdentityRequired");
  if (terms.base_right_ref && !valid_source_ref(*terms.base_right_ref))
    return common_failure("InvalidFoundationSource", terms.base_right_ref->id);
  if (!terms.base_right_ref && terms.mobile_carrier_refs.empty())
    return common_failure("FoundationResourceUnavailable");
  for (const auto *refs :
       {&terms.mobile_carrier_refs, &terms.asset_contribution_refs,
        &terms.supply_contract_refs,
        &terms.former_responsibility_dispositions}) {
    auto valid = validate_sources(*refs);
    if (!valid.ok)
      return valid;
  }
  return common_success();
}

// 顺序也是原输入的一部分；不排序/去重后把不同条款称同一版本。
bool same_sources(const std::vector<SourceRef> &a,
                  const std::vector<SourceRef> &b) {
  if (a.size() != b.size())
    return false;
  for (std::size_t i = 0; i < a.size(); ++i)
    if (!same_source_ref(a[i], b[i]))
      return false;
  return true;
}
} // namespace

const char *foundation_kind_tag(FoundationKind kind) {
  switch (kind) {
  case FoundationKind::licensed_charter:
    return "licensed_charter";
  case FoundationKind::voluntary_association:
    return "voluntary_association";
  case FoundationKind::secession:
    return "secession";
  }
  return nullptr;
}

std::optional<FoundationKind> foundation_kind_from_tag(const StableId &tag) {
  if (tag == "licensed_charter")
    return FoundationKind::licensed_charter;
  if (tag == "voluntary_association")
    return FoundationKind::voluntary_association;
  if (tag == "secession")
    return FoundationKind::secession;
  return std::nullopt;
}

Status validate_foundation_terms_shape(const FoundationTerms &terms) {
  auto valid = validate_inputs(terms);
  if (!valid.ok)
    return valid;
  // 原要求允许单一创办人；真实接受与 cohort 去重人数由原消费者反查，
  // 不能新增 accepted_member_refs 必须非空或 refs.size()==人数的规则。
  return validate_sources(terms.accepted_member_refs);
}

Status
validate_foundation_intent_terms_shape(const FoundationIntentTerms &terms) {
  return validate_inputs(terms);
}

bool foundation_terms_match_intent(const FoundationTerms &terms,
                                   const FoundationIntentTerms &intent) {
  return terms.kind == intent.kind &&
         terms.organization_kind == intent.organization_kind &&
         terms.constitution_id == intent.constitution_id &&
         terms.initial_office_id == intent.initial_office_id &&
         terms.initial_grant_template_id == intent.initial_grant_template_id &&
         terms.founder == intent.founder &&
         terms.base_right_ref.has_value() ==
             intent.base_right_ref.has_value() &&
         (!terms.base_right_ref ||
          same_source_ref(*terms.base_right_ref, *intent.base_right_ref)) &&
         same_sources(terms.mobile_carrier_refs, intent.mobile_carrier_refs) &&
         same_sources(terms.asset_contribution_refs,
                      intent.asset_contribution_refs) &&
         same_sources(terms.supply_contract_refs,
                      intent.supply_contract_refs) &&
         same_sources(terms.former_responsibility_dispositions,
                      intent.former_responsibility_dispositions);
}

Status validate_founding_requirement_shape(const FoundingRequirement &rule) {
  if (rule.min_accepted_people < 1 || rule.runway_ticks <= 0 ||
      rule.required_base_kind.empty())
    return common_failure("InvalidFoundingRequirement");
  return common_success();
}

Outcome<const FoundingRequirement *> read_original_founding_requirement(
    const PoliticalFoundationRequirementCatalog &catalog,
    const CatalogIdentity &expected, const StableId &organization_kind) {
  if (expected.content_digest.empty() ||
      !same_catalog(catalog.identity, expected))
    return {false, nullptr,
            common_failure("FoundationCatalogIdentityMismatch").error};
  if (organization_kind.empty())
    return {false, nullptr,
            common_failure("FoundationTermsIdentityRequired").error};
  const auto *rule =
      catalog.requirements_by_organization_kind.find(organization_kind);
  if (!rule)
    return {false, nullptr,
            common_failure("FoundationRequirementMissing", organization_kind)
                .error};
  auto valid = validate_founding_requirement_shape(*rule);
  if (!valid.ok)
    return {false, nullptr, valid.error};
  return {true, rule, {}};
}
} // namespace hanmo::resident
