// 现有人物/对白只投影当面可接洽的人、本人已知邀约和原Task派工，不增加组织管理总表。
#pragma once
#include "resident/domains/organization/membership_offer.hpp"
namespace hanmo::resident {
class EconomicSupplyPolicyRead;
struct OrganizationContractCommand;
// 每次最多64条有限人物事项；provider为空是当前场所，非空须是当前真实对白对象。
Outcome<BoundaryValue> query_named_membership_entries(const WorldRead&,const PreparedCatalog&,
    const ActivityRead&,const MembershipReleaseRead&,const StableId& observer,
    const StableId& place,const StableId& provider_actor_id={},
    const std::function<bool(const OrganizationContractCommand&)>& can_accept_organization={},const EconomicSupplyPolicyRead* issued_supply=nullptr);
}
