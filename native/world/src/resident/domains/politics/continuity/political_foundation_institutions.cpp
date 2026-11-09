#include "resident/kernel/catalog_section_observation.hpp"
#include "political_foundation_institutions.hpp"
#include "resident/domains/organization/grant_authority.hpp"
#include "resident/domains/organization/grant_terms_definition.hpp"
#include "resident/kernel/original_catalog_identity.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
namespace {
// 新定义的有限列表只接受原Scope五种tag；不把其存在解释为当前控制或产权。
bool scope_kind(ScopeRef::Kind kind) {
  switch (kind) {
  case ScopeRef::Kind::site:
  case ScopeRef::Kind::region:
  case ScopeRef::Kind::force:
  case ScopeRef::Kind::account:
  case ScopeRef::Kind::activity:
    return true;
  }
  return false;
}
Status template_definition(const Grant &g) {
  auto valid = validate_authored_grant_terms_definition(g);
  if (!valid.ok)
    return valid;
  if (g.id.empty() || g.office_id.empty())
    return common_failure("FoundationGrantTemplateIdentityRequired", g.id);
  std::set<StableId> commands, wallets, inventories;
  std::set<std::pair<ScopeRef::Kind, StableId>> scopes;
  for (const auto &kind : g.allowed_command_kinds)
    if (kind.empty() || !commands.insert(kind).second)
      return common_failure("InvalidFoundationGrantTemplateCommands", g.id);
  for (const auto &item : g.scopes)
    if (!scope_kind(item.kind) || item.id.empty() ||
        !scopes.insert({item.kind, item.id}).second)
      return common_failure("InvalidFoundationGrantTemplateScopes", g.id);
  for (const auto &id : g.wallet_ids)
    if (id.empty() || !wallets.insert(id).second)
      return common_failure("InvalidFoundationGrantTemplateWallets", g.id);
  for (const auto &id : g.inventory_ids)
    if (id.empty() || !inventories.insert(id).second)
      return common_failure("InvalidFoundationGrantTemplateInventories", g.id);
  return common_success();
}
// 每个定义与模板双向对应同一章程，不从另一现役组织查不到时回退。
Status constitution_definition(const FoundationConstitutionDefinition &c) {
  if (c.id.empty() || c.organization_kind.empty() ||
      c.initial_offices.empty() || c.initial_grant_templates.empty())
    return common_failure("FoundationConstitutionDefinitionRequired", c.id);
  for (const auto &row : c.initial_offices.rows()) {
    const auto &office = row.second;
    if (row.first != office.id || office.id.empty())
      return common_failure("InvalidFoundationOfficeDefinition", row.first);
    std::set<StableId> templates;
    for (const auto &id : office.grant_template_ids) {
      const auto *grant = c.initial_grant_templates.find(id);
      if (!grant || grant->office_id != office.id ||
          !templates.insert(id).second)
        return common_failure("FoundationOfficeGrantTemplateMismatch", id);
    }
  }
  for (const auto &row : c.initial_grant_templates.rows()) {
    const auto &grant = row.second;
    auto valid = template_definition(grant);
    if (!valid.ok)
      return valid;
    const auto *office = c.initial_offices.find(grant.office_id);
    if (row.first != grant.id || !office ||
        std::find(office->grant_template_ids.begin(),
                  office->grant_template_ids.end(),
                  grant.id) == office->grant_template_ids.end())
      return common_failure("FoundationOfficeGrantTemplateMismatch", row.first);
  }
  return common_success();
}
} // namespace
Status validate_foundation_institution_catalog(
    const FoundationInstitutionCatalog &catalog) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 89, "FoundationInstitutionCatalog", "validate_foundation_institution_catalog", &catalog);

  if (catalog.identity.content_digest.empty())
    return common_failure("FoundationCatalogIdentityMismatch");
  for (const auto &row : catalog.constitutions.rows()) {
    if (row.first != row.second.id)
      return common_failure("FoundationConstitutionIdentityMismatch",
                            row.first);
    auto valid = constitution_definition(row.second);
    if (!valid.ok)
      return valid;
  }
  return common_success();
}
Outcome<FoundationInitialInstitutionView>
read_foundation_initial_institution(const FoundationInstitutionCatalog &catalog,
                                    const CatalogIdentity &expected,
                                    const FoundationIntentTerms &terms) {
  if (expected.content_digest.empty() ||
      !same_original_catalog_identity(catalog.identity, expected))
    return {
        false, {}, common_failure("FoundationCatalogIdentityMismatch").error};
  auto valid = validate_foundation_intent_terms_shape(terms);
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto *constitution = catalog.constitutions.find(terms.constitution_id);
  if (!constitution || constitution->id != terms.constitution_id ||
      constitution->organization_kind != terms.organization_kind)
    return {false,
            {},
            common_failure("FoundationConstitutionDefinitionMissing",
                           terms.constitution_id)
                .error};
  valid = constitution_definition(*constitution);
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto *office =
      constitution->initial_offices.find(terms.initial_office_id);
  const auto *grant = constitution->initial_grant_templates.find(
      terms.initial_grant_template_id);
  if (!office || !grant || grant->office_id != office->id)
    return {false,
            {},
            common_failure("FoundationInitialInstitutionMismatch",
                           terms.initial_office_id)
                .error};
  return {true, {constitution, office, grant}, {}};
}
Status validate_foundation_grant_template_terms(
    const FoundationInitialInstitutionView &view, const Grant &proposed) {
  // 借值必须真属于同一锁内定义；裸指针三元组不能变成免校验许可。
  if (!view.constitution || !view.office || !view.grant_template ||
      view.constitution->initial_offices.find(view.office->id) != view.office ||
      view.constitution->initial_grant_templates.find(
          view.grant_template->id) != view.grant_template ||
      view.grant_template->office_id != view.office->id)
    return common_failure("FoundationInitialInstitutionMismatch");
  auto valid = constitution_definition(*view.constitution);
  if (!valid.ok)
    return valid;
  valid = template_definition(proposed);
  if (!valid.ok)
    return valid;
  if (!grant_terms_within(proposed, *view.grant_template) ||
      proposed.continuity_policy_id !=
          view.grant_template->continuity_policy_id)
    return common_failure("FoundationGrantExceedsConstitution", proposed.id);
  return common_success();
}
} // namespace hanmo::resident
