// 青川 Field 守门专用作者许可；不进入一般内容规则，也不代表无人控制道路。
#pragma once
#include "political_passage_decision.hpp"
#include "resident/domains/organization/organization_model.hpp"
#include "resident/domains/travel/travel.hpp"
namespace hanmo::resident {
// 每行就是唯一许可来源；没有另造授权事实或可改写的政策状态。
struct FieldGroupPassagePolicy {
  StableId id;
  Integer version = 1;
  PassageBoundaryRef boundary;
  StableId organization_id;
};
struct FieldGroupPassageCatalog {
  CatalogIdentity identity;
  Integer schema_version = 1;
  StableId provenance;
  std::vector<FieldGroupPassagePolicy> policies;
};
// 只接受已批准的四条有向道路×组织 army/allowed/no-tariff 记录。
// identity 必须来自实际 Prepared 内容封印，不能由块内声明替代。
Outcome<FieldGroupPassageCatalog> decode_field_group_passage_catalog(
    const BoundaryValue &, const CatalogIdentity &, const OrganizationCatalog &,
    const TravelCatalog &);
Status validate_field_group_passage_catalog(const FieldGroupPassageCatalog &,
                                            const OrganizationCatalog &,
                                            const TravelCatalog &);
// 同步借用目录行；不存在的组合是未获此 fixture 许可，不是默认开放。
Outcome<const FieldGroupPassagePolicy *>
read_field_group_passage_policy(const FieldGroupPassageCatalog &,
                                const PassageBoundaryRef &,
                                const StableId &organization_id);
} // namespace hanmo::resident
