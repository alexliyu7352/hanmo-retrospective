// 原M-CONTINUITY锁目录的已实现成立叶；不是第二World根或完整能力声明。
#pragma once
#include "political_continuity_contract.hpp"
#include "political_foundation_institutions.hpp"
#include "political_foundation_requirements_codec.hpp"
namespace hanmo::resident {
// 两叶仍各自使用原唯一字段表和原业务validator。LeadershipRule的资格、
// 代理/争议policy目录及真实reader尚缺，不能以本型替代完整required准入。
struct PoliticalContinuityCatalog {
  PoliticalFoundationRequirementCatalog founding_requirements;
  FoundationInstitutionCatalog founding_institutions;
};
Status validate_political_continuity_catalog(
    const PoliticalContinuityCatalog &,
    const CatalogIdentity &actual_locked_identity);
Status visit_political_continuity_catalog(CodecVisitor &,
                                          PoliticalContinuityCatalog &);
Status emit_political_continuity_catalog_projection(
    ProjectionWriter &, std::string_view, const PoliticalContinuityCatalog &,
    const CatalogIdentity &actual_locked_identity);
Outcome<BoundaryValue> encode_political_continuity_catalog(
    const PoliticalContinuityCatalog &,
    const CatalogIdentity &actual_locked_identity);
Outcome<PoliticalContinuityCatalog> decode_political_continuity_catalog(
    const BoundaryValue &, const CatalogIdentity &actual_locked_identity);
// 只返回同一fresh候选的空程序身体，不分配事实/人/Office/Grant，且不注册ready。
// 外层仍须保真实clock/锁目录属于该World，最终完整来源门不可省略。
Outcome<PoliticalContinuityState> make_fresh_political_continuity_body(
    const CommonStore &, const PoliticalContinuityCatalog &,
    const CatalogIdentity &, const CurrentMomentRead &);
} // namespace hanmo::resident
