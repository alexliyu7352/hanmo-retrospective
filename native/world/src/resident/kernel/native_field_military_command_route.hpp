// Field 出征只组合原 CampaignDepart 与原 S1 Army opening；不另造军令或旅程。
#pragma once
#include "resident/domains/military/military_model.hpp"
namespace hanmo::resident {
struct WorldBundle;
struct CompleteExactWorldReadBindings;
// 仅真实 Complete/Field 的原出征命令；其他军令仍走既有各自 owner。
bool native_field_campaign_departure_selected(const WorldBundle &,
                                              const MilitaryCommand &) noexcept;
// 报价借同一 Complete 原军权限，不借旧非精确形成来源替代当前军团。
Outcome<MilitaryQuote> preview_native_field_campaign_departure(
    const WorldBundle &, const CompleteExactWorldReadBindings &,
    const StableId &, const CampaignDepart &);
// 原 producer 自己接受一次 CampaignDepart；不得先执行 legacy instant 再重放。
Outcome<MilitaryEffect> apply_native_field_campaign_departure_in_candidate(
    WorldBundle &, const CompleteExactWorldReadBindings &, const StableId &,
    const CampaignDepart &);
} // namespace hanmo::resident
