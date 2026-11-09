// 三个原 typed 根的有限物理选择；这里只验布局，不声称业务来源已齐。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct PoliticalBudgetState;
struct PublicMarketAuthorityState;
struct PublicMarketAuthorityCatalog;
struct PoliticalTreatyTermsState;
struct PoliticalTreatyTermsCatalog;
struct PoliticalOriginalRootSelection {
  bool budgets = false, public_market = false, treaty_terms = false;
};
// 每原模块仅 /1；重复、未知版本拒绝。未选不从旧外交/财政或字段存在反推。
Outcome<PoliticalOriginalRootSelection>
selected_political_original_roots(const CatalogIdentity &);
// 作者目录是否存在只由原选择决定；完整外部原目录规则另有强制 validator。
Status
validate_political_original_catalog_layout(const CatalogIdentity &,
                                           const PublicMarketAuthorityCatalog *,
                                           const PoliticalTreatyTermsCatalog *);
// 对双向缺席、两原锁目录全身份与原schema作精确检查；不验外部权利或预算政策。
Status validate_political_original_root_layout(
    const CatalogIdentity &, const PoliticalBudgetState *,
    const PublicMarketAuthorityState *, const PublicMarketAuthorityCatalog *,
    const PoliticalTreatyTermsState *, const PoliticalTreatyTermsCatalog *);
} // namespace hanmo::resident
