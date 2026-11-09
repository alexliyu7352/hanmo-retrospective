#include "field_contact_origin.hpp"
#include "field_exact_contact_source.hpp"
#include "field_session_build.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
namespace hanmo::resident {
Outcome<GeneratedBattleField> regenerate_field_contact_battle(
    const TravelCatalog &travel, const FieldBattleCatalog &catalog,
    const FieldContactOrigin &origin, const FieldExactContactReadContext *exact,
    ExactArithmeticBudget budget) {
  if (const auto legacy = std::get_if<FieldRoadContact>(&origin))
    return regenerate_field_road_battle(travel, catalog, *legacy);
  if (!exact || &exact->travel != &travel)
    return {false,
            {},
            common_failure("FieldExactContactOriginalContextRequired").error};
  return regenerate_field_exact_road_battle(
      *exact, std::get<FieldExactGroupContactOrigin>(origin).actual_contact,
      catalog, budget);
}
Status
validate_field_contact_origin_head(const CommonStore &common,
                                   const FieldBattleContactRecord &record) {
  auto parent = read_actual_common_fact_position(
      common, {"military", "field_battle_contact", record.contact_id});
  if (!parent.ok)
    return {false, {}, parent.error};
  if (const auto legacy = std::get_if<FieldRoadContact>(&record.source)) {
    if (!legacy->contact.road || legacy->army_ids[0].empty() ||
        legacy->army_ids[0] == legacy->army_ids[1])
      return common_failure("FieldContactSourceMismatch", record.contact_id);
    return common_success();
  }
  const auto &ref =
      std::get<FieldExactGroupContactOrigin>(record.source).actual_contact;
  if (ref.domain != "travel" || ref.kind != "movement_contact")
    return common_failure("FieldExactContactOriginalSourceRequired",
                          record.contact_id);
  auto source = read_actual_common_fact_position(common, ref);
  if (!source.ok)
    return {false, {}, source.error};
  if (source.value >= parent.value)
    return common_failure("FieldExactContactSourceOrderMismatch",
                          record.contact_id);
  return common_success();
}
} // namespace hanmo::resident
