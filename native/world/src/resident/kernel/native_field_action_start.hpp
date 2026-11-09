// 原 Office/Form/Order 的瞬态准入借口；不新增持久行动或第二份完成源。
#pragma once
#include "resident/domains/military/military_model.hpp"
#include <memory>
namespace hanmo::resident {
struct WorldBundle;
class MilitaryExactWriteLease;
// 真 Complete/Field 选择只分派现有两类固定服务行动。
bool native_field_elapsed_action_selected(const WorldBundle &,
                                          const MilitaryCommand &) noexcept;
// 有限 Field 原任职/受令/成军报价借真实 Military scope，保原当面与权源首错。
bool native_field_original_military_quote_selected(
    const WorldBundle &, const MilitaryCommand &) noexcept;
Outcome<MilitaryQuote> preview_native_field_original_military_command(
    const WorldBundle &, const StableId &actor, const MilitaryCommand &);
// 原受令即时 writer 使用同 World authority_time；失败撤原外层整个候选。
Outcome<MilitaryEffect> apply_native_field_campaign_acceptance_in_candidate(
    WorldBundle &, const StableId &actor, const CampaignAccept &);
// 必须在 Common action 分配/采用之前借原 owner，避免用半个开工状态重新建全图。
Outcome<std::shared_ptr<MilitaryExactWriteLease>>
prepare_native_field_elapsed_action_start(WorldBundle &,
                                          const MilitaryCommand &);
// Common 原 action 已采用一次后，仅调用相配原 exact writer；失败由整候选撤销。
Status start_native_field_elapsed_action_in_candidate(WorldBundle &,
                                                      MilitaryExactWriteLease &,
                                                      const ActionCoreView &,
                                                      const MilitaryCommand &,
                                                      const MilitaryQuote &);
} // namespace hanmo::resident
