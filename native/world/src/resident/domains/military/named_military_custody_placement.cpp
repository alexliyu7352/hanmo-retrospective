#include "resident/domains/military/military_model.hpp"
// 此叶不调用完整释放 validator，避免历史位置复读递归；不能单凭本叶解除看管门。
#include "named_military_custody_placement.hpp"
#include "named_military_custody_terms.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
namespace hanmo::resident {
using namespace movement_detail;
bool is_named_military_custody_placement_source(const SourceRef &source) noexcept {
  return source.domain == "military" && source.kind == "military_named_custody_released";
}
bool OriginalNamedMilitaryCustodyPlacementRead::bound_to(
    const CommonRead &common, const MovementExactState &movement) const noexcept {
  return &common == static_cast<const CommonRead *>(&context_.common) && &movement == &movement_;
}
Outcome<const NamedMilitaryCustodyRelease *>
OriginalNamedMilitaryCustodyPlacementRead::read_named_custody_placement(
    const SourceRef &source, const StableId &actor) const {
  return guard<const NamedMilitaryCustodyRelease *>([&] {
    take(validate_named_military_custody_policy(context_));
    if (!take(selected_named_military_custody(context_.identity)) ||
        !is_named_military_custody_placement_source(source) || !context_.clock ||
        !context_.common.actor(actor)) fail("NamedMilitaryCustodyPlacementSourceRequired", source.id);
    const NamedMilitaryCustodyRelease *release = nullptr;
    for (const auto &row : context_.military.named_custody.closures) if (row.id == source.id) {
      if (release) fail("NamedMilitaryCustodyReleaseRepeated", source.id);
      release = &row;
    }
    const NamedMilitaryCustodyTerm *term = nullptr;
    if (release) for (const auto &row : context_.military.named_custody.terms)
      if (row.id == release->term_id) {
        if (term) fail("NamedMilitaryCustodyTermRepeated", row.id);
        term = &row;
      }
    if (!release || !term || release->actor_id != actor || term->actor_id != actor ||
        release->recorded_tick != release->released_at.half_day_tick ||
        take(compare_world_moments(release->released_at, term->due_at)) != 0 || release->keeper_army_id.empty() ||
        release->origin_sources.empty()) fail("NamedMilitaryCustodyPlacementSourceRequired", source.id);
    const auto now = take(context_.clock->read_current_moment());
    if (take(compare_world_moments(release->released_at, now)) > 0)
      fail("NamedMilitaryCustodyReleaseFutureSource", source.id);
    const auto order = take(read_actual_common_fact_position(context_.common, source));
    const auto term_order = take(read_actual_common_fact_position(context_.common,
        SourceRef{"military", "military_named_custody_term", term->id}));
    const auto capture_order = take(read_actual_common_fact_position(context_.common, term->capture_source));
    const auto prior_order = take(read_actual_common_fact_position(context_.common, release->prior_custody_source));
    if (capture_order >= term_order || term_order >= order || prior_order >= order)
      fail("NamedMilitaryCustodyReleaseOrderMismatch", source.id);
    const SourceRef *actual_source = nullptr;
    if (const auto *anchor = std::get_if<OriginalMovementAnchor>(&release->released_anchor))
      actual_source = &anchor->actual_source;
    else if (const auto *anchor = std::get_if<RoadFractionalAnchor>(&release->released_anchor))
      actual_source = &anchor->actual_source;
    if (!actual_source || actual_source->domain != source.domain ||
        actual_source->kind != source.kind || actual_source->id != source.id)
      fail("NamedMilitaryCustodyPlacementAnchorRequired", source.id);
    take(project_movement_location(release->released_anchor));
    return release;
  });
}
Outcome<std::vector<const NamedMilitaryCustodyTerm *>>
OriginalNamedMilitaryCustodyPlacementRead::read_named_custody_capture_history(const StableId &actor) const {
  return guard<std::vector<const NamedMilitaryCustodyTerm *>>([&] {
    take(validate_named_military_custody_terms(context_));
    if (!context_.common.actor(actor)) fail("UnknownActor", actor);
    std::vector<const NamedMilitaryCustodyTerm *> out;
    for (const auto &term : context_.military.named_custody.terms)
      if (term.actor_id == actor) out.push_back(&term);
    return out;
  });
}
Outcome<std::vector<const NamedMilitaryCustodyRelease *>>
OriginalNamedMilitaryCustodyPlacementRead::read_named_custody_placement_history(const StableId &actor) const {
  return guard<std::vector<const NamedMilitaryCustodyRelease *>>([&] {
    std::vector<const NamedMilitaryCustodyRelease *> out;
    for (const auto &release : context_.military.named_custody.closures)
      if (release.actor_id == actor)
        out.push_back(take(read_named_custody_placement(
            {"military", "military_named_custody_released", release.id}, actor)));
    return out;
  });
}
} // namespace hanmo::resident
