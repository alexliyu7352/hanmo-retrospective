// 原身体实际应用事实前缀的只读叶；不保存前缀、不以 effective Moment
// 推应用顺序。
#pragma once
#include "character_condition_read.hpp"
namespace hanmo::resident {
// consumer 必须原 Common 真头，caller 先核其实际 payload、Actor 与 Moment。
// 仅已选新应用头能力可读；借原完整 cause/reducer，不替消费业务授许可。
Outcome<CharacterConditionAtHead>
read_character_condition_before_application_fact(
    const CharacterConditionSources &, const StableId &actor,
    const SourceRef &actual_consumer, const WorldMoment &actual_consumer_at);
} // namespace hanmo::resident
