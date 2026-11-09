#include "injury_field_coverage_v1.hpp"
#include "resident/domains/military/military_model.hpp"
#include "character_condition_read.hpp"
#include "injury_original_results_v1.hpp"
#include "injury_source_links_v1.hpp"
#include "injury_sources_v1.hpp"
#include "resident/domains/military/field_battle/field_segment_settlement.hpp"
namespace hanmo::resident {
Status validate_field_injury_source_coverage_v1(
    const CharacterConditionSources &body,
    const CharacterInjurySourcesStateV1 &sources, const MilitaryState &military,
    const FieldBattleWorldState &field) {
  auto valid = validate_field_segment_settlement_sources(
      body.common, military, field, body.injury_field_clock);
  if (!valid.ok)
    return valid;
  valid = validate_character_injury_source_links_v1(body.common, body.state,
                                                    body.catalog, sources);
  if (!valid.ok)
    return valid;
  // 正向认证真实原回执，不能仅凭应用表中的领域字符串通过。
  for (const auto &row : sources.applications.rows()) {
    const auto *battle = std::get_if<CharacterBattleReceiptSourceV1>(
        &row.second.original_source);
    if (!battle || battle->original_receipt.domain != "military")
      continue;
    const auto *receipt =
        body.state.character_condition_extensions.applied_receipts.find(
            row.first);
    if (!receipt)
      return common_failure("CharacterInjuryFieldReceiptMissing", row.first);
    valid = validate_field_injury_original_result_v1(body, sources, *receipt,
                                                     military, field);
    if (!valid.ok)
      return valid;
  }
  // 从全部真实段反扫，不从伤势应用表反推来源完整性；非终局段同样需要落账。
  for (const auto &session : field.sessions) {
    for (const auto &settlement : session.second.settlements) {
      const MilitaryCasualtyFact *casualty = nullptr;
      for (const auto &row : military.casualty_facts)
        if (row.battle_id == settlement.fact_id) {
          if (casualty)
            return common_failure("CharacterInjuryRepeatedFieldCasualty",
                                  settlement.fact_id);
          casualty = &row;
        }
      if (!casualty)
        return common_failure("CharacterInjuryFieldCasualtyMissing",
                              settlement.fact_id);
      for (const auto &change : casualty->actor_changes) {
        if (change.status != "wounded")
          continue;
        std::size_t count = 0;
        for (const auto &row : sources.applications.rows()) {
          const auto *battle = std::get_if<CharacterBattleReceiptSourceV1>(
              &row.second.original_source);
          if (battle && battle->original_receipt.domain == "military" &&
              battle->original_receipt.kind == "military_casualties" &&
              battle->original_receipt.id == casualty->id &&
              row.second.actor_id == change.actor_id)
            ++count;
        }
        if (count != 1)
          return common_failure(
              "CharacterInjuryFieldParticipantCoverageMismatch", casualty->id);
      }
    }
  }
  return common_success();
}
} // namespace hanmo::resident
