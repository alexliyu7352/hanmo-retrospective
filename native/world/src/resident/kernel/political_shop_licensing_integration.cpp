// 自持军史/互动借口先于context构造；回滚后不再解引用旧scope，下一调用重建同World读。
#include "political_shop_licensing_integration.hpp"
#include "exact_complete_world.hpp"
#include "native_complete_world_core.hpp"
#include "native_private_shop_exact_sources.hpp"
#include "native_private_shop_operations.hpp"
#include "resident/domains/commerce/private_shop.hpp"
#include "resident/domains/military/military_roster_history.hpp"
#include "resident/domains/politics/licensing/political_shop_licensing_bootstrap2.hpp"
#include "resident/domains/politics/licensing/political_shop_licensing_result_bindings.hpp"
#include "world_bundle.hpp"
#include <limits>
namespace hanmo::resident {
namespace {
FamilyDeathOriginRead life_source(const WorldBundle &w,
                                  const DynamicStepInput *phase) {
  if (!w.family_life_enabled())
    return {};
  return [&w, phase](const StableId &actor)
             -> Outcome<std::optional<FamilyDeathOrigin>> {
    auto source =
        phase ? query_military_actor_death(w.common, w.military, actor, *phase)
              : query_military_actor_death(w.common, w.military, actor);
    if (!source.ok)
      return {false, {}, source.error};
    if (!source.value)
      return {true, std::nullopt, {}};
    const auto &d = *source.value;
    return {
        true,
        FamilyDeathOrigin{d.actor_id, d.casualty_fact_id, d.battle_id, d.tick},
        {}};
  };
}
Status available(const WorldBundle &w, bool initialized) {
  if (!w.dynamic_enabled() || !w.politics_enabled() || !w.catalog->common ||
      !w.catalog->organization || !w.catalog->organization->enabled ||
      !w.catalog->local_economy || !w.catalog->interaction ||
      !w.catalog->private_shops || !w.catalog->political_shop_licensing)
    return common_failure("PoliticalShopLicensingProfileRequired");
  const auto shop_version = w.catalog->private_shops->version;
  Integer selected_law = 0;
  for (const auto &module : w.catalog->identity.module_versions)
    if (module.first == political_shop_licensing_module) {
      if (selected_law || (module.second != "1" && module.second != "2"))
        return common_failure("UnsupportedPoliticalShopPairVersion");
      selected_law = module.second == "2" ? 2 : 1;
    }
  if (w.catalog->political_shop_licensing->version != 1 ||
      !(((shop_version == 1 || shop_version == 2) && selected_law == 1) ||
        (shop_version == 3 && selected_law == 2)) ||
      (initialized && w.political_shop_licensing.version != selected_law))
    return common_failure("UnsupportedPoliticalShopPairVersion");
  if (selected_law == 2 && w.political_shop_registry_successions_enabled())
    return common_failure("PoliticalShopExactRegistrySourceRequired", {},
                          ErrorKind::unsupported_capability);
  if (w.political_shop_registry_successions_enabled() &&
      (!w.political_takeover_enabled() ||
       !w.catalog->political_shop_registry_successions ||
       w.catalog->political_shop_registry_successions->version != 1 ||
       (initialized && !w.political_shop_registry_successions_seed_complete)))
    return common_failure("PoliticalShopRegistrySuccessionProfileRequired");
  if (initialized && !w.political_shop_licensing_seed_complete)
    return common_failure("PoliticalShopInitialRegistryRequired");
  if (w.political_takeover_enabled() &&
      (!w.political_takeover_seed_complete || !w.catalog->military))
    return common_failure("PoliticalShopControlHistoryUnavailable");
  return common_success();
}
// 正式 outer 只取一次完整同World图；域内 create 仍仅借已持 core，绝不回
// factory。
class PoliticalShopCurrentReadOwner final : public CheckpointAllocatedObject {
  std::unique_ptr<PhaseCurrentReadScope> phase_;
  std::unique_ptr<CompleteExactWorldBindingOwner> complete_;
  std::unique_ptr<NativePrivateShopExactSources> exact_;
  std::unique_ptr<PoliticalShopLicensingWorldAccess> access_;

public:
  PoliticalShopCurrentReadOwner() = default;
  PoliticalShopCurrentReadOwner(const PoliticalShopCurrentReadOwner &) = delete;
  static Outcome<std::unique_ptr<PoliticalShopCurrentReadOwner>>
  create(const WorldBundle &w) {
    auto out = std::unique_ptr<PoliticalShopCurrentReadOwner>(
        new PoliticalShopCurrentReadOwner);
    if (w.political_shop_licensing.version == 2) {
      if (!w.read_stamps || !w.world_timeline)
        return {false,
                {},
                common_failure("PoliticalShopExactWorldClockRequired").error};
      if (!w.has_active_read_phase()) {
        auto now = w.actual_phase_moment();
        if (!now.ok)
          return {false, {}, std::move(now.error)};
        auto phase = w.enter_phase_read_context(
            "political_shop_original_read", "political_shop_licensing",
            {now.value, now.value}, now.value);
        if (!phase.ok)
          return {false, {}, std::move(phase.error)};
        out->phase_ = std::move(phase.value);
      }
      auto complete = w.complete_exact_time_bindings();
      if (!complete.ok)
        return {false, {}, std::move(complete.error)};
      if (!complete.value || !complete.value->reads().original_core)
        return {
            false, {}, common_failure("PoliticalShopExactCoreRequired").error};
      out->complete_ = std::move(complete.value);
      auto exact = NativePrivateShopExactSources::create(
          *out->complete_->reads().original_core);
      if (!exact.ok)
        return {false, {}, std::move(exact.error)};
      out->exact_ = std::move(exact.value);
    }
    auto access = PoliticalShopLicensingWorldAccess::create(w, nullptr,
                                                            out->exact_.get());
    if (!access.ok)
      return {false, {}, std::move(access.error)};
    out->access_ = std::move(access.value);
    return {true, std::move(out), {}};
  }
  const PoliticalShopLicensingWorldAccess &access() const noexcept {
    return *access_;
  }
};
} // namespace
PoliticalShopLicensingWorldAccess::PoliticalShopLicensingWorldAccess(
    const WorldBundle &w, const DynamicStepInput *phase,
    const NativePrivateShopExactSources *exact)
    : military_(w.political_takeover_enabled()
                    ? std::optional<MilitaryAccess>{w.services.military_access(
                          phase ? std::optional<Integer>{phase->to_tick}
                                : std::nullopt,
                          phase)}
                    : std::nullopt),
      control_(
          military_
              ? std::optional<
                    PoliticalShopControlContext>{{w.common,
                                                  *w.catalog
                                                       ->political_takeover,
                                                  w.political_takeover,
                                                  w.military,
                                                  *w.catalog->military,
                                                  *military_, phase}}
              : std::nullopt),
      interaction_(w.services.interaction_access()),
      original_context_{w.common,
                        *w.catalog->common,
                        w.politics,
                        *w.catalog->politics,
                        w.organization,
                        *w.catalog->organization,
                        w.local_economy,
                        w.services,
                        w.interaction,
                        *w.catalog->interaction,
                        interaction_,
                        *w.catalog->political_shop_licensing,
                        w.political_shop_licensing,
                        w.catalog->private_shops->premises,
                        control_ ? &*control_ : nullptr,
                        life_source(w, phase),
                        phase,
                        nullptr,
                        exact ? &exact->authority_clock() : nullptr,
                        exact ? &exact->body() : nullptr,
                        exact ? &exact->clock() : nullptr,
                        exact ? &exact->opening() : nullptr,
                        nullptr,
                        exact ? &exact->read_boundary() : nullptr},
      registry_context_(
          w.political_shop_registry_successions_enabled()
              ? std::optional<
                    PoliticalShopRegistryContext>{{original_context_, *control_,
                                                   *w.catalog
                                                        ->political_shop_registry_successions,
                                                   w.political_shop_registry_successions}}
              : std::nullopt),
      registry_(
          registry_context_
              ? std::optional<PoliticalShopRegistryAccess>{*registry_context_}
              : std::nullopt),
      context_([this] {
        auto c = original_context_;
        c.registry_successions = registry_ ? &*registry_ : nullptr;
        return c;
      }()),
      permissions_(context_) {}
Outcome<std::unique_ptr<PoliticalShopLicensingWorldAccess>>
PoliticalShopLicensingWorldAccess::create(
    const WorldBundle &w, const DynamicStepInput *phase,
    const NativePrivateShopExactSources *exact) {
  if (phase && (w.common.tick() < 0 ||
                w.common.tick() == std::numeric_limits<Integer>::max() ||
                phase->from_tick != w.common.tick() ||
                phase->to_tick != w.common.tick() + 1))
    return {
        false, {}, common_failure("PoliticalShopControlPhaseMismatch").error};
  auto ready = available(w, true);
  if (!ready.ok)
    return {false, {}, ready.error};
  if (w.political_shop_licensing.version == 2) {
    if (!exact || !exact->bound_to(w))
      return {false,
              {},
              common_failure("PoliticalShopExactOwningSourcesRequired", {},
                             ErrorKind::unsupported_capability)
                  .error};
    auto binding = exact->validate_bindings();
    if (!binding.ok)
      return {false, {}, std::move(binding.error)};
  } else if (exact) {
    return {
        false, {}, common_failure("UnexpectedPoliticalShopExactSources").error};
  }
  return {true,
          std::unique_ptr<PoliticalShopLicensingWorldAccess>{
              new PoliticalShopLicensingWorldAccess(w, phase, exact)},
          {}};
}
Status initialize_political_shop_licensing_world_in_candidate(WorldBundle &w) {
  auto ready = available(w, false);
  if (!ready.ok)
    return ready;
  if (w.political_shop_licensing_seed_complete)
    return common_failure("PoliticalShopInitialHistoryExists");
  if (w.political_shop_licensing_receipts)
    return common_failure("PoliticalShopLicensingReceiptsInitialHistoryExists");
  if (w.political_shop_registry_successions_enabled() &&
      (w.political_shop_registry_successions_seed_complete ||
       w.political_shop_registry_successions.version != 1 ||
       !w.political_shop_registry_successions.acceptances.empty()))
    return common_failure("PoliticalShopRegistryInitialHistoryExists");
  auto selected =
      selected_native_political_shop_licensing_version(w.catalog->identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (selected.value == 2) {
    const auto &state = w.political_shop_licensing;
    if (!w.world_timeline || w.has_active_read_phase() ||
        (state.version != 1 && state.version != 2) ||
        !state.registries.empty() || !state.warrants.empty() ||
        !state.releases.empty() || !state.clearances.empty() ||
        !state.permits.empty() || !state.revocations.empty())
      return common_failure("PoliticalShopExactInitialBoundaryRequired");
    // 只在真正fresh candidate明确选择原空根的物理版；恢复永不调用本口。
    w.political_shop_licensing.version = 2;
  }
  PoliticalShopLicensingWorldAccess read(w);
  NativeOrganizationAuthorityClock authority(w);
  WorldCurrentMomentRead moment(w);
  auto initialized =
      selected.value == 2
          ? initialize_political_shop_licensing_bootstrap2_in_candidate(
                w.common, w.political_shop_licensing, read.context(), authority,
                moment)
          : initialize_political_shop_licensing_in_candidate(
                w.common, w.political_shop_licensing, read.context());
  if (!initialized.ok)
    return initialized;
  w.political_shop_licensing_seed_complete = true;
  if (w.catalog->political_shop_licensing_receipts)
    w.political_shop_licensing_receipts.emplace();
  if (w.political_shop_registry_successions_enabled()) {
    auto checked =
        validate_political_shop_registry_successions(*read.registry_context());
    if (!checked.ok)
      return checked;
    w.political_shop_registry_successions_seed_complete = true;
  }
  return common_success();
}
Status validate_political_shop_licensing_world(const WorldBundle &w) {
  const auto *catalog = w.catalog->political_shop_licensing.get();
  const auto *state = w.political_shop_licensing_seed_complete
                          ? &w.political_shop_licensing
                          : nullptr;
  auto layout = validate_political_shop_licensing_layout(
      w.common, w.catalog->identity.module_versions, catalog, state);
  if (!layout.ok)
    return layout;
  auto receipts = validate_political_shop_licensing_receipt_layout(
      w.catalog->identity.module_versions,
      w.catalog->political_shop_licensing_receipts,
      w.political_shop_licensing_receipts
          ? &*w.political_shop_licensing_receipts
          : nullptr);
  if (!receipts.ok)
    return receipts;
  if (!catalog) {
    const auto &s = w.political_shop_licensing;
    return s.version == 1 && s.registries.empty() && s.warrants.empty() &&
                   s.releases.empty() && s.clearances.empty() &&
                   s.permits.empty() && s.revocations.empty()
               ? common_success()
               : common_failure("UnexpectedPoliticalShopLicensingState");
  }
  auto read = PoliticalShopCurrentReadOwner::create(w);
  if (!read.ok)
    return {false, {}, read.error};
  auto valid =
      validate_political_shop_licensing(read.value->access().context());
  if (!valid.ok)
    return valid;
  if (w.political_shop_licensing_receipts)
    return validate_political_shop_licensing_player_results(
        w.common, w.political_shop_licensing,
        *w.political_shop_licensing_receipts, [](const CommitResult &r) {
          return r.details && std::holds_alternative<Done>(r.details->data);
        });
  return validate_political_shop_licensing_receipts(
      read.value->access().context(), [](const CommitResult &r) {
        return r.details && std::holds_alternative<Done>(r.details->data);
      });
}
Outcome<OwnedPoliticalShopLicensingQuote>
prepare_political_shop_licensing_world_owned(
    const WorldBundle &w, const StableId &actor,
    const PoliticalShopLicensingCommand &command) {
  auto guard = command_common_guard(
      w, actor, political_shop_licensing_command_kind(command));
  if (!guard.ok)
    return {false, {}, std::move(guard.error)};
  auto read = PoliticalShopCurrentReadOwner::create(w);
  if (!read.ok)
    return {false, {}, std::move(read.error)};
  return preview_political_shop_licensing_owned(read.value->access().context(),
                                                actor, command);
}
Outcome<PoliticalShopLicensingQuote> prepare_political_shop_licensing_world(
    const WorldBundle &w, const StableId &actor,
    const PoliticalShopLicensingCommand &command) {
  auto guard = command_common_guard(
      w, actor, political_shop_licensing_command_kind(command));
  if (!guard.ok)
    return {false, {}, guard.error};
  auto read = PoliticalShopCurrentReadOwner::create(w);
  if (!read.ok)
    return {false, {}, read.error};
  return preview_political_shop_licensing(read.value->access().context(), actor,
                                          command);
}
Outcome<OwnedPoliticalShopLicensingEffects>
commit_political_shop_licensing_world_owned(
    WorldBundle &w, const StableId &actor,
    const PoliticalShopLicensingCommand &command,
    const PoliticalShopLicensingQuote &quote) {
  auto guard = command_common_guard(
      w, actor, political_shop_licensing_command_kind(command));
  if (!guard.ok)
    return {false, {}, guard.error};
  auto read = PoliticalShopCurrentReadOwner::create(w);
  if (!read.ok)
    return {false, {}, read.error};
  return apply_political_shop_licensing_owned_in_candidate(
      w.common, w.political_shop_licensing, read.value->access().context(),
      actor, command, quote);
}
Outcome<std::vector<StableId>> commit_political_shop_licensing_world(
    WorldBundle &w, const StableId &actor,
    const PoliticalShopLicensingCommand &command,
    const PoliticalShopLicensingQuote &quote) {
  auto owned =
      commit_political_shop_licensing_world_owned(w, actor, command, quote);
  if (!owned.ok)
    return {false, {}, std::move(owned.error)};
  return {true, {owned.value.fact_ids.begin(), owned.value.fact_ids.end()}, {}};
}
Outcome<BoundaryValue>
query_political_shop_licensing_world(const WorldBundle &w,
                                     const StableId &place, Integer cursor) {
  auto read = PoliticalShopCurrentReadOwner::create(w);
  if (!read.ok)
    return {false, {}, read.error};
  return query_political_shop_licensing(read.value->access().context(), place,
                                        cursor);
}
} // namespace hanmo::resident
