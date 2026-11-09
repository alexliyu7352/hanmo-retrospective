// 冷作者定义的唯一版本化读口；不形成活动、能力或可发送Family结果。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct BoundaryValue;
struct GrowthChallengeDefinition;
struct GrowthCatalog;
// 仅精确局部2组合；完整实际CatalogIdentity及作者根仍由中央安装门证明。
Status validate_growth_family_etiquette_selection2(const CatalogIdentity &);
// 原旧定义接受域，未知kind/新version/原坏题面均明确拒绝。
Outcome<GrowthChallengeDefinition> decode_original_growth_challenge_definition(
    const BoundaryValue &, const StableId &kind, const CommonCatalog &);
// 新Family定义必须显式正版本，仍使用原3题/3选项/公开文本/原canonical SHA。
Outcome<GrowthChallengeDefinition> decode_family_etiquette_definition2(
    const BoundaryValue &, const CommonCatalog &, const CatalogIdentity &);
// 新静态目录沿同一个原Growth loader；不将空目录视为完整Family作者安装成功。
Outcome<GrowthCatalog> decode_growth_family_etiquette_catalog2(
    const BoundaryValue &, const CommonCatalog &, const CatalogIdentity &,
    bool regional_life = false, bool innate_learning = false);
} // namespace hanmo::resident
