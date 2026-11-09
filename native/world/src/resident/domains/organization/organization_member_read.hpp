// 当前成员选择与原欠薪读取；不是历史接受证明，也不新增持久来源。
#pragma once
#include "organization_model.hpp"
namespace hanmo::resident {
// Engaged only when the original active-then-last selection found a real row.
// On debt failure the selected row remains available and view.error is the
// unchanged original debt error; view.value is not a successful projection.
struct OrganizationSelectedMemberRead {
    const OrgMemberState* selected_member=nullptr;
    Outcome<OrganizationMemberView> view;
};
// All inputs must belong to the same current candidate. The selected row is
// borrowed only while that candidate is unchanged: any mutation, rollback,
// clone/move/restore or root replacement invalidates this read. No Common cut,
// durable role occurrence or historical authority is proved by this result.
// nullopt means no selected member, never a collapsed debt failure.
std::optional<OrganizationSelectedMemberRead> read_current_organization_member(
    const CommonStore&,const OrganizationState&,const OrganizationCatalog&,
    const LocalEconomyState&,const StableId& actor_id,const StableId& organization_id);
} // namespace hanmo::resident
