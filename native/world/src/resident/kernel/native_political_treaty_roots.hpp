// 原条约唯一 World/Prepared
// 根准入；形状与完整来源门分离，不能以空根冒业务能力。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct BoundaryValue;
struct WorldBundle;
struct TreatyTermsOriginalRead;
struct PoliticalTreatyResourceReadContext2;
// 只读锁内有限版本；未选返回零，重复与未知版本不降级。
Outcome<Integer>
selected_native_political_treaty_version(const CatalogIdentity &);
// 实际作者 job 传入原 terms 正文与同一 scenario；必须提供真外部目录原读。
Status
prepare_native_political_treaty_catalog(const BoundaryValue &actual_terms_input,
                                        const BoundaryValue &actual_scenario,
                                        PreparedCatalog &,
                                        const TreatyTermsOriginalRead &);
// 只检目录/锁/明版配对；真正 full catalog/source reader 仍必须独立运行。
Status validate_native_political_treaty_catalog_layout(const PreparedCatalog &);
Status initialize_native_political_treaty_seed(const PreparedCatalog &,
                                               InitialWorldSeed &);
Status validate_native_political_treaty_root_layout(const WorldBundle &);
// 明示 /2 的 full caller 必须绑定 World 真表与 Prepared 真
// mapping，不能借另一个已验 context。
Status validate_native_political_treaty_resources(
    const WorldBundle &, const PoliticalTreatyResourceReadContext2 &);
} // namespace hanmo::resident
