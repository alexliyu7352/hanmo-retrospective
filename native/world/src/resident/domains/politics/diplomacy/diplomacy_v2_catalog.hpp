// 仅明确外交v2持有的作者委任策略；不改v1目录字段。
#pragma once
#include "resident/domains/organization/organization_model.hpp"
namespace hanmo::resident {
struct DiplomaticDelegationPolicy {
    StableId id, mission_policy_id, terms_id, issuer_office_id, envoy_actor_id;
};
struct DiplomacyV2Catalog {
    Integer version = 2;
    OrderedTable<DiplomaticDelegationPolicy> delegation_policies;
};
}
