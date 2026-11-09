// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
// 认可、本人条件同意及死后控制分别核原来源；旁支关系不转移财产、公职或隐秘知识。
#include "collateral_control_world.hpp"
#include "biological_control_world.hpp"
#include "historical_player.hpp"
#include "resident/domains/interaction/personal_dialogues/known_personal_dialogue_world.hpp"
#include "resident/kernel/world_bundle.hpp"
#include "voluntary_succession.hpp"
#include <charconv>
namespace hanmo::resident {
namespace {
Status profile(const WorldBundle &w) {
  const bool enabled = w.family_collateral_legacy_enabled();
  Integer markers = 0, legacy_markers = 0;
  for (const auto &m : w.catalog->identity.module_versions) {
    if (m.first == "family_collateral_legacy") {
      ++markers;
      if (m.second != "1")
        return common_failure("UnsupportedCollateralControlVersion");
    }
    if (m.first == "family_legacy") {
      ++legacy_markers;
      if (enabled && m.second != "3")
        return common_failure("CollateralControlLegacyVersionRequired");
    }
  }
  if (enabled != w.family_collateral_legacy_seed_complete ||
      bool(w.catalog->family_collateral_legacy) != enabled ||
      markers != (enabled ? 1 : 0))
    return common_failure("CollateralControlProfileMismatch");
  if (!enabled) {
    if (!w.family_collateral_legacy.recognitions.empty() ||
        !w.family_collateral_legacy.recognition_commits.empty() ||
        !w.family_collateral_legacy.legacy_commits.empty() ||
        !w.family_collateral_legacy.proposals.empty() ||
        !w.family_collateral_legacy.replies.empty() ||
        (w.catalog->family_legacy && w.catalog->family_legacy->version == 3))
      return common_failure("CollateralControlProfileRequired");
    return common_success();
  }
  if (!w.dynamic_enabled() || !w.family_life_enabled() ||
      !w.family_legacy_enabled() || !w.catalog->family->enabled ||
      !w.catalog->family_lineage || w.catalog->family_legacy->version != 3 ||
      legacy_markers != 1 || !w.catalog->known_personal_dialogues ||
      !w.interaction.known_personal_dialogues ||
      w.catalog->interaction->common.get() != w.catalog->common.get())
    return common_failure("CollateralControlDependenciesRequired");
  return common_success();
}
Integer serial(const CommonStore &c, const StableId &id) {
  auto colon = id.find(':');
  if (colon == StableId::npos)
    return -1;
  auto text = id.substr(colon + 1);
  Integer n = 0;
  auto read = std::from_chars(text.data(), text.data() + text.size(), n);
  return read.ec == std::errc{} && read.ptr == text.data() + text.size() &&
                 n > 0 && n < c.state().next_instance_id &&
                 std::to_string(n) == text
             ? n
             : -1;
}
// 同tick控制来源与实际声明头的实例分配序/Common顺序必须同侧；折叠仍用原唯一控制链。
Outcome<StableId> player_before(const WorldBundle &w, Integer tick,
                                const StableId &fact) {
  const auto &c = w.common;
  const auto *head = c.state().facts.find(fact);
  const auto number = serial(c, fact);
  const auto &order = c.state().facts.order();
  const auto at = std::find(order.begin(), order.end(), fact);
  const bool recognition = head && head->domain == "family_collateral_legacy" &&
                           head->kind == "family_collateral_recognized" &&
                           fact.rfind("family_collateral:", 0) == 0;
  const bool decision = head && head->domain == "family_legacy" &&
                        (head->kind == "family_legacy_control_consent" ||
                         head->kind == "family_legacy_control_revocation") &&
                        fact.rfind("family_legacy:", 0) == 0;
  if (!head || head->id != fact || (!recognition && !decision) || number < 1 ||
      at == order.end() || tick < 0 || tick > c.tick())
    return {
        false,
        {},
        common_failure("CollateralControlPlayerBoundaryRequired", fact).error};
  auto side = [&](const StableId &id, Integer changed) -> Status {
    const auto n = serial(c, id);
    const auto where = std::find(order.begin(), order.end(), id);
    if (n < 1 || where == order.end() || n == number ||
        ((n < number) != (where < at)) ||
        (where < at ? changed > tick : changed < tick))
      return common_failure("CollateralControlPlayerOrderMismatch", id);
    return common_success();
  };
  for (const auto &id : w.family.successions) {
    auto step = family_succession_step(w.family, id);
    if (!step.ok)
      return {false, {}, step.error};
    auto valid = side(id, step.value.tick);
    if (!valid.ok)
      return {false, {}, valid.error};
  }
  for (const auto &row : w.family_legacy.activations.rows()) {
    auto valid = side(row.first, row.second.tick);
    if (!valid.ok)
      return {false, {}, valid.error};
  }
  return family_player_before_fact_with_legacy(c, w.family, w.family_legacy,
                                               tick, fact);
}
std::optional<DiscipleControlRelationContext> disciple(const WorldBundle &w) {
  if (!w.family_disciple_legacy_enabled())
    return {};
  return DiscipleControlRelationContext{w.common,
                                        *w.catalog->common,
                                        w.family_apprenticeship,
                                        *w.catalog->family_legacy,
                                        *w.catalog->family_disciple_legacy,
                                        w.family_apprenticeship_access()};
}
Outcome<std::shared_ptr<BiologicalControlWorldAccess>>
biological(const WorldBundle &w) {
  return w.family_biological_legacy_enabled()
             ? BiologicalControlWorldAccess::create(w)
             : Outcome<std::shared_ptr<BiologicalControlWorldAccess>>{
                   true, {}, {}};
}
} // namespace
CollateralControlWorldAccess::CollateralControlWorldAccess(
    const WorldBundle &w,
    std::shared_ptr<KnownPersonalDialogueWorldAccess> meetings)
    : world_(w), meetings_(std::move(meetings)),
      recognition_{
          {w.common, *w.catalog->common, *w.catalog->family_lineage,
           w.family_birth_enabled() ? &w.family_birth : nullptr, &w.family_life,
           w.family_death_read()},
          *w.catalog->family_collateral_legacy,
          *w.catalog->family_legacy,
          *w.catalog->interaction,
          [this](const StableId &id) -> Outcome<KnownPersonalDialogueTemplate> {
            const auto *t =
                known_personal_dialogue_template(meetings_->catalog(), id);
            if (!t)
              return {
                  false,
                  {},
                  common_failure("CollateralControlTemplateSourceRequired", id)
                      .error};
            return {true, *t, {}};
          },
          &meetings_->read(),
          [this](Integer tick, const StableId &fact) {
            return player_before(world_, tick, fact);
          }},
      context_{recognition_, w.family_collateral_legacy} {}
Outcome<std::shared_ptr<CollateralControlWorldAccess>>
CollateralControlWorldAccess::create(const WorldBundle &w) {
  auto valid = profile(w);
  if (!valid.ok)
    return {false, {}, valid.error};
  if (!w.family_collateral_legacy_enabled())
    return {
        false, {}, common_failure("CollateralControlProfileRequired").error};
  auto meetings = KnownPersonalDialogueWorldAccess::create(w);
  if (!meetings.ok)
    return {false, {}, meetings.error};
  auto out = std::shared_ptr<CollateralControlWorldAccess>(
      new CollateralControlWorldAccess(w, std::move(meetings.value)));
  valid = validate_collateral_control_context(out->context());
  return valid.ok
             ? Outcome<std::shared_ptr<CollateralControlWorldAccess>>{true,
                                                                      std::move(
                                                                          out),
                                                                      {}}
             : Outcome<std::shared_ptr<CollateralControlWorldAccess>>{
                   false, {}, valid.error};
}
Status validate_family_collateral_recognition_world(const WorldBundle &w) {
  auto valid = profile(w);
  if (!valid.ok || !w.family_collateral_legacy_enabled())
    return valid;
  auto access = CollateralControlWorldAccess::create(w);
  if (!access.ok)
    return {false, {}, access.error};
  valid = validate_collateral_recognition_history(access.value->recognition(),
                                                  w.family_collateral_legacy);
  if (!valid.ok)
    return valid;
  return validate_collateral_recognition_result_bindings(
      w.common, w.family_collateral_legacy);
}
Status validate_family_collateral_world(const WorldBundle &w) {
  auto valid = profile(w);
  if (!valid.ok || !w.family_collateral_legacy_enabled())
    return valid;
  auto access = CollateralControlWorldAccess::create(w);
  if (!access.ok)
    return {false, {}, access.error};
  auto bio = biological(w);
  if (!bio.ok)
    return {false, {}, bio.error};
  auto relation = disciple(w);
  valid = validate_family_legacy(
      w.common, w.family, w.family_life, w.family_death_read(),
      *w.catalog->common, *w.catalog->interaction, *w.catalog->family_legacy,
      w.family_legacy, w.family_legacy_playable_read(),
      relation ? &*relation : nullptr,
      bio.value ? &bio.value->context() : nullptr, &access.value->context());
  if (!valid.ok)
    return valid;
  return validate_collateral_control_result_bindings(
      w.common, w.family_legacy, *w.catalog->family_collateral_legacy,
      w.family_collateral_legacy);
}
Status bind_family_collateral_result_world(
    WorldBundle &w, const FamilyLegacyState *before_legacy,
    const CollateralControlState *before, const StableId &actor,
    Integer request, const CommitResult &result) {
  if (!w.family_collateral_legacy_enabled())
    return before_legacy || before
               ? common_failure("CollateralControlProfileRequired")
               : common_success();
  auto valid = profile(w);
  if (!valid.ok)
    return valid;
  if (!before_legacy || !before)
    return common_failure("CollateralControlBeforeStateRequired");
  valid = bind_collateral_recognition_result_in_candidate(
      w.common, *before, w.family_collateral_legacy, actor, request, result);
  if (!valid.ok)
    return valid;
  return bind_collateral_control_result_in_candidate(
      w.common, *before_legacy, w.family_legacy,
      *w.catalog->family_collateral_legacy, *before, w.family_collateral_legacy,
      actor, request, result);
}
Outcome<CollateralControlQuote>
preview_family_collateral_world(const WorldBundle &w, const StableId &actor,
                                const CollateralControlCommand &command) {
  auto access = CollateralControlWorldAccess::create(w);
  if (!access.ok)
    return {false, {}, access.error};
  auto bio = biological(w);
  if (!bio.ok)
    return {false, {}, bio.error};
  auto relation = disciple(w);
  auto interaction = w.services.interaction_access();
  return preview_collateral_control_command(
      access.value->context(), w.family, w.family_life, w.family_death_read(),
      w.interaction, interaction, w.family_legacy,
      w.family_legacy_playable_read(), actor, command,
      relation ? &*relation : nullptr,
      bio.value ? &bio.value->context() : nullptr);
}
Outcome<FamilyEffect>
apply_family_collateral_world(WorldBundle &w, const StableId &actor,
                              const CollateralControlCommand &command,
                              const CollateralControlQuote &quote) {
  auto access = CollateralControlWorldAccess::create(w);
  if (!access.ok)
    return {false, {}, access.error};
  auto bio = biological(w);
  if (!bio.ok)
    return {false, {}, bio.error};
  auto relation = disciple(w);
  auto interaction = w.services.interaction_access();
  return apply_collateral_control_command(
      w.common, access.value->context(), w.family_collateral_legacy, w.family,
      w.family_life, w.family_death_read(), w.interaction, interaction,
      w.family_legacy, w.family_legacy_playable_read(), actor, command, quote,
      relation ? &*relation : nullptr,
      bio.value ? &bio.value->context() : nullptr);
}
Outcome<BoundaryValue> query_family_collateral_world(const WorldBundle &w,
                                                     Integer page) {
  auto access = CollateralControlWorldAccess::create(w);
  if (!access.ok)
    return {false, {}, access.error};
  auto bio = biological(w);
  if (!bio.ok)
    return {false, {}, bio.error};
  auto relation = disciple(w);
  auto interaction = w.services.interaction_access();
  return query_collateral_control_page(
      access.value->context(), w.family, w.family_life, w.family_death_read(),
      w.interaction, interaction, w.family_legacy,
      w.family_legacy_playable_read(), page, relation ? &*relation : nullptr,
      bio.value ? &bio.value->context() : nullptr);
}
} // namespace hanmo::resident
