#include "grant_terms_definition.hpp"
#include <set>
namespace hanmo::resident {
Status validate_authored_grant_terms_definition(const Grant &g) {
  if (g.amount_limit < 0 || g.valid_from < 0 ||
      (g.valid_until && *g.valid_until < g.valid_from) ||
      g.allowed_command_kinds.empty() || g.scopes.empty() ||
      (g.continuity_policy_id != "continue_under_organization" &&
       g.continuity_policy_id != "stop_when_vacant"))
    return common_failure("InvalidAuthorityGrant", g.id);
  return common_success();
}
Status validate_runtime_grant_terms_definition(const Grant &g,
                                               bool require_id) {
  if ((require_id && g.id.empty()) || g.id.size() > 256 || g.amount_limit < 0 ||
      g.valid_from < 0 || !g.valid_until || *g.valid_until < g.valid_from ||
      g.allowed_command_kinds.empty() || g.allowed_command_kinds.size() > 128 ||
      g.scopes.empty() || g.scopes.size() > 128 ||
      (g.continuity_policy_id != "continue_under_organization" &&
       g.continuity_policy_id != "stop_when_vacant"))
    return common_failure("InvalidRuntimeGrant", g.id);
  std::set<StableId> commands;
  std::set<std::pair<ScopeRef::Kind, StableId>> scopes;
  for (const auto &kind : g.allowed_command_kinds)
    if (kind.empty() || !commands.insert(kind).second)
      return common_failure("InvalidRuntimeGrantCommands", g.id);
  for (const auto &item : g.scopes)
    if (item.id.empty() || !scopes.insert({item.kind, item.id}).second)
      return common_failure("InvalidRuntimeGrantScopes", g.id);
  return common_success();
}
} // namespace hanmo::resident
