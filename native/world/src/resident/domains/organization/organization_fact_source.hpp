// 原append_org_fact的唯一具名采用口；只写同Common头和原OrganizationFact，不提交候选。
#pragma once
#include "organization_model.hpp"
namespace hanmo::resident {
Outcome<StableId> append_organization_fact_in_candidate(CommonStore&,OrganizationState&,OrganizationFact);
}
