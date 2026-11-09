// 原生命目录显式选版；身体原因与岁检分别保持唯一原 owner。
#pragma once
#include "resident/domains/family/family_death.hpp"
#include "resident/domains/family/life_age_exact_phase.hpp"
namespace hanmo::resident {
struct PreparedCatalog;
Outcome<FamilyDeathPolicy> decode_native_family_life_policy(const BoundaryValue&);
Status append_native_life_age_module(const BoundaryValue& scenario,CatalogIdentity&);
Status prepare_native_life_age_catalog(const BoundaryValue& scenario,PreparedCatalog&);
Status validate_native_life_age_catalog(const PreparedCatalog&);
}
