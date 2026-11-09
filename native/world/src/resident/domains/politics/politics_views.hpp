// 政治页面只读有限当地公职和本人已知任命；不借权限开放全国实时状态。
#pragma once
#include "politics_model.hpp"
#include "resident/domains/social/social_model.hpp"
namespace hanmo::resident {
struct PoliticalNavigationRead;
struct PoliticsViewIndex;
// 仅显式撤职来源profile提供同步附加门；缺省保持原页面规则。
using PoliticalDismissalPreviewRead =
    std::function<Status(const StableId &, const PoliticalDismissCommand &)>;
// 当面见证为送达证据；其余历史必须在原Social已知报告中找到实际接收时刻。
bool political_fact_known(const CommonStore &, const PoliticsState &,
                          const SocialState &, const StableId &observer,
                          const StableId &fact_id);
// 完整模式按原事实精确时刻与实际钟读已知性；错误不伪装成未知事实。
Outcome<bool>
read_political_fact_known(const CommonStore &, const PoliticsState &,
                          const SocialState &, const StableId &observer,
                          const StableId &fact_id,
                          const OrganizationAuthorityClock *clock = nullptr);
Outcome<BoundaryValue> query_politics_entries(
    const CommonStore &, const CommonCatalog &, const PoliticsState &,
    const PoliticsCatalog &, const OrganizationState &,
    const OrganizationCatalog &, const LocalEconomyState &, const SocialState &,
    const ActivityRead &, const StableId &observer, const StableId &place,
    const PoliticalNavigationRead *navigation = nullptr,
    const PoliticsViewIndex *index = nullptr,
    const PoliticalDismissalPreviewRead &dismissal_preview = {},
    const PoliticalOrganizationRuntimeReadContext *runtime = nullptr);
// 中央绑定叶可复用同一严格命令边界；未知字段与命令不默认成功。
Outcome<PoliticsCommand> decode_politics_command(const StableId &kind,
                                                 const BoundaryValue &params);
BoundaryValue encode_politics_command(const PoliticsCommand &);
StableId politics_command_kind(const PoliticsCommand &);
} // namespace hanmo::resident
