// P§15.4 原初始 Office/Grant 的锁内定义；目录定义不等于现役任职或授权。
#pragma once
#include "political_foundation_terms.hpp"
#include "resident/domains/organization/organization_model.hpp"
namespace hanmo::resident {
// id 是章程内的原职务定义地址，不预分配未来 OfficeState/holder/epoch。
struct FoundationInitialOfficeDefinition {
  StableId id;
  std::vector<StableId> grant_template_ids;
};
struct FoundationConstitutionDefinition {
  StableId id, organization_kind;
  OrderedTable<FoundationInitialOfficeDefinition> initial_offices;
  // Grant.id/office_id 在此仅是模板/职务定义地址；不得直接当现行Grant读取。
  // 其余字段原样沿原 Grant，没有额外的权限字符串、默认额度或假资源。
  OrderedTable<Grant> initial_grant_templates;
};
// 只属于原 continuity 锁目录，不新增一份 World 根或复制现役 Office/Grant。
struct FoundationInstitutionCatalog {
  CatalogIdentity identity;
  OrderedTable<FoundationConstitutionDefinition> constitutions;
};
struct FoundationInitialInstitutionView {
  const FoundationConstitutionDefinition *constitution = nullptr;
  const FoundationInitialOfficeDefinition *office = nullptr;
  const Grant *grant_template = nullptr;
};
Status
validate_foundation_institution_catalog(const FoundationInstitutionCatalog &);
// 精确反读同锁、同形态、同章程内的三个输入地址；缺实际作者行明确拒绝。
Outcome<FoundationInitialInstitutionView>
read_foundation_initial_institution(const FoundationInstitutionCatalog &,
                                    const CatalogIdentity &,
                                    const FoundationIntentTerms &);
// 新 grant 的运行 ID/Office ID 必须由后继唯一 writer 分配与绑定，此口不授任职。
// 只按原 Grant 子集规则核初始条款不能超过章程模板，continuity 不得偷换。
Status validate_foundation_grant_template_terms(
    const FoundationInitialInstitutionView &, const Grant &proposed_terms);
} // namespace hanmo::resident

namespace hanmo::resident {
// 本目录候选尚未接入 political_continuity 的共享作者入口；不新增 World 段。
Status visit_foundation_institution_catalog(CodecVisitor &,
                                            FoundationInstitutionCatalog &);
// 同一原字段表的字段级出口，只供拥有原对象的组合codec使用。
Status emit_foundation_institution_catalog_fields(
    ProjectionWriter &, const FoundationInstitutionCatalog &);
Status emit_foundation_institution_catalog_projection(
    ProjectionWriter &, std::string_view, const FoundationInstitutionCatalog &);
Outcome<BoundaryValue>
encode_foundation_institution_catalog(const FoundationInstitutionCatalog &);
Outcome<FoundationInstitutionCatalog> decode_foundation_institution_catalog(
    const BoundaryValue &, const CatalogIdentity &actual_locked_identity);
} // namespace hanmo::resident
