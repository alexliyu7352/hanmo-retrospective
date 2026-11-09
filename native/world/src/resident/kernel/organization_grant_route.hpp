// 待共享 owner 审合；不注册公共 variant，未构成 P1 完成。
#pragma once
#include "resident/domains/organization/grant_authority.hpp"
namespace hanmo::resident {
struct WorldBundle;
struct BoundaryValue;
bool organization_grant_command_kind(const StableId &);
// Wire fields mirror the one original GrantChangeCommand; no alternate grant
// DTO.
Outcome<GrantChangeCommand>
decode_organization_grant_command(const StableId &kind,
                                  const BoundaryValue &params);
Status preview_organization_grant_command(const WorldBundle &,
                                          const StableId &actor,
                                          const StableId &kind,
                                          const GrantChangeCommand &);
Outcome<OrganizationEffects>
apply_organization_grant_command(WorldBundle &, const StableId &actor,
                                 const StableId &kind,
                                 const GrantChangeCommand &);
} // namespace hanmo::resident
