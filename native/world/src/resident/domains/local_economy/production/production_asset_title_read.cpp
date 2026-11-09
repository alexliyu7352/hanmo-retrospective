// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
// 反读原安装与本人命令所封口的实际交割，不从初始作者 Owner 猜今日产权。
#include "production_asset_title_read.hpp"
#include "facility_construction_adoption.hpp"
#include "resident/kernel/command_types.hpp"
#include "resident/kernel/common_command_sources.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/original_catalog_identity.hpp"
#include <array>
#include <span>

namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
int compare(const WorldMoment &a, const WorldMoment &b) {
  return take(compare_world_moments(a, b));
}
// 本域 typed 表的每一行都必须有唯一原 Common 头，反向覆盖在实际遍历中核。
std::size_t header(const CommonStore &common, const StableId &id,
                   std::string_view kind) {
  const auto *value = common.state().facts.find(id);
  if (!value || value->id != id || value->domain != "local_economy" ||
      value->kind != kind)
    fail("ProductionAssetLifecycleHeaderRequired", id);
  const auto &order = common.state().facts.order();
  for (std::size_t n = 0; n < order.size(); ++n)
    if (order[n] == id)
      return n;
  fail("ProductionAssetLifecycleOrderRequired", id);
}
// 结果必须反读真正成功缓存/历史的本人和准确结果列表，不借任意成功命令。
void command(const CommonStore &common, const SourceRef &source,
             const StableId &actor, const WorldMoment &at,
             std::span<const std::string_view> facts,
             std::span<const std::string_view> receipts) {
  const auto actual = take(read_common_command_source(common, source));
  if (actual.history->actor_id != actor ||
      actual.result->kind != "accept_property_transfer" ||
      actual.result->tick != at.half_day_tick || !actual.result->details)
    fail("ProductionAssetActualPartyCommandRequired", source.id);
  const auto *effects =
      std::get_if<LocalEconomyEffects>(&actual.result->details->data);
  if (!effects || effects->fact_ids.size() != facts.size() ||
      effects->receipt_ids.size() != receipts.size())
    fail("ProductionAssetCommandResultsMismatch", source.id);
  for (std::size_t n = 0; n < facts.size(); ++n)
    if (effects->fact_ids[n] != facts[n])
      fail("ProductionAssetCommandResultsMismatch", source.id);
  for (std::size_t n = 0; n < receipts.size(); ++n)
    if (effects->receipt_ids[n] != receipts[n])
      fail("ProductionAssetCommandResultsMismatch", source.id);
}
// 初始头必须来自唯一实际物理安装和同内容锁。土地等无该原源的分支仍
// 明确 required；不能仅凭 ProductionAsset 目录行存在发今日产权证明。
ProductionAssetTitleRead initial(const ProductionAssetLifecycleReadContext &x,
                                 const StableId &id) {
  const auto &installation = x.installation;
  const auto now = take(x.clock.read_current_moment());
  const auto &metadata = installation.common.state().metadata;
  if (x.state.schema_version != 1 ||
      !same_original_catalog_identity(x.identity,
                                      installation.catalog.identity) ||
      x.identity.content_id != metadata.content_id ||
      x.identity.content_version != metadata.content_version ||
      x.identity.ruleset_id != metadata.ruleset_id ||
      x.identity.content_digest != metadata.content_digest ||
      now.half_day_tick != installation.common.tick() ||
      compare(now, installation.current))
    fail("ProductionAssetLifecycleContextMismatch", id);
  take(validate_adopted_economic_facility_definitions(
      installation.common, installation.state, installation.catalog));
  ProductionAssetTitleRead out;
  out.original_asset = installation.production_assets.assets.find(id);
  if (!out.original_asset || out.original_asset->id != id)
    fail("UnknownProductionAsset", id);
  for (const auto &facility : installation.state.facilities)
    if (facility.asset_owner_domain == "local_economy" &&
        facility.original_asset_ref == id &&
        std::holds_alternative<InstalledPhysicalFacility>(
            facility.initialization_basis)) {
      if (out.original_facility)
        fail("ProductionAssetTitleInstallationAmbiguous", id);
      take(validate_economic_facility_installation_source(installation,
                                                          facility));
      out.original_facility = &facility;
    }
  if (!out.original_facility)
    fail("ProductionAssetOriginalTitleInstallationRequired", id);
  out.initial_owner = &out.original_asset->owner;
  return out;
}
// 逐唯一 ID 反读，拒同 ID 两份条款；借值与 lifecycle 根同寿命。
template <class T, class Allocator, class Key>
const T &unique(const std::vector<T, Allocator> &rows, const StableId &id,
                Key key) {
  const T *found = nullptr;
  for (const auto &row : rows)
    if (key(row) == id) {
      if (found)
        fail("DuplicateProductionAssetLifecycleSource", id);
      found = &row;
    }
  if (!found)
    fail("ProductionAssetLifecyclePayloadRequired", id);
  return *found;
}
const ProductionAssetTransferOffer &
offer(const ProductionAssetLifecycleState &state, const StableId &id) {
  return unique(state.offers, id, [](const auto &row) -> const StableId & {
    return row.offer_id;
  });
}
const ProductionAssetTransferAcceptance &
acceptance(const ProductionAssetLifecycleState &state, const StableId &id) {
  return unique(state.acceptances, id, [](const auto &row) -> const StableId & {
    return row.acceptance_id;
  });
}
const ProductionAssetTitleTransfer &
transfer(const ProductionAssetLifecycleState &state, const StableId &id) {
  return unique(state.transfers, id, [](const auto &row) -> const StableId & {
    return row.transfer_id;
  });
}
// 只接受同一个真实产权头的原卖方，旧报价不得穿过任何一次实际转权。
void same_title(const ProductionAssetTitleRead &title,
                const ProductionAssetTransferTerms &terms) {
  if (!production_asset_title_owned_by(title, Owner::Kind::actor,
                                       terms.seller_actor_id) ||
      !production_asset_title_source_equal(title, terms.previous_title_source))
    fail("ProductionAssetTransferTitleChanged", terms.asset_ref);
}
} // namespace

// 每个钱包仍是指定原当事人的原私钱包；产权转让不重写钱包 owner。
Outcome<ProductionAssetPrivateParties>
read_production_asset_private_parties(const CommonStore &common,
                                      const StableId &seller_id,
                                      const StableId &buyer_id) {
  ProductionAssetPrivateParties out;
  out.seller = common.actor(seller_id);
  out.buyer = common.actor(buyer_id);
  if (!out.seller || !out.buyer || seller_id == buyer_id)
    return {
        false,
        {},
        common_failure("ProductionAssetOriginalPrivatePartiesRequired").error};
  out.payer = common.wallet(out.buyer->wallet_id);
  out.payee = common.wallet(out.seller->wallet_id);
  if (!out.payer || !out.payee || out.payer->id == out.payee->id ||
      out.payer->owner.kind != Owner::Kind::actor ||
      out.payer->owner.id != buyer_id || out.payer->owner.extra ||
      out.payee->owner.kind != Owner::Kind::actor ||
      out.payee->owner.id != seller_id || out.payee->owner.extra)
    return {
        false,
        {},
        common_failure("ProductionAssetOriginalPrivatePartiesRequired").error};
  return {true, out, {}};
}
bool production_asset_title_owned_by(const ProductionAssetTitleRead &title,
                                     Owner::Kind kind,
                                     std::string_view id) noexcept {
  if (!title.original_asset || !title.original_facility ||
      !title.initial_owner || id.empty())
    return false;
  if (title.last_transfer_offer)
    return title.last_transfer && kind == Owner::Kind::actor &&
           id == title.last_transfer_offer->terms.buyer_actor_id;
  return !title.last_transfer && kind == title.initial_owner->kind &&
         id == title.initial_owner->id;
}
bool production_asset_title_source_equal(
    const ProductionAssetTitleRead &title,
    const MomentOwnerSource &source) noexcept {
  if (source.source_schema != 1 || source.source_ref.domain != "local_economy")
    return false;
  if (title.last_transfer)
    return source.source_ref.kind == "production_asset_title_transferred" &&
           source.source_ref.id == title.last_transfer->transfer_id;
  if (!title.original_facility)
    return false;
  const auto *installed = std::get_if<InstalledPhysicalFacility>(
      &title.original_facility->initialization_basis);
  return installed && source.source_ref.kind == "economic_facility_installed" &&
         source.source_ref.id ==
             installed->installation_evidence.installation_id;
}
Outcome<ProductionAssetTitleRead> read_current_production_asset_title(
    const ProductionAssetLifecycleReadContext &x, const StableId &id) {
  try {
    auto title = initial(x, id);
    const auto &common = x.installation.common;
    for (const auto &row : x.state.offers)
      header(common, row.offer_id, "production_asset_transfer_offered");
    for (const auto &row : x.state.acceptances)
      header(common, row.acceptance_id, "production_asset_transfer_accepted");
    for (const auto &row : x.state.transfers)
      header(common, row.transfer_id, "production_asset_title_transferred");
    WorldMoment previous = std::get<InstalledPhysicalFacility>(
                               title.original_facility->initialization_basis)
                               .installation_evidence.installed_at;
    const auto now = take(x.clock.read_current_moment());
    for (const auto &entry : common.state().facts.rows()) {
      const auto &h = entry.second;
      if (h.domain != "local_economy")
        continue;
      if (h.kind == "production_asset_transfer_offered") {
        const auto &o = offer(x.state, entry.first);
        if (o.terms.asset_ref != id)
          continue;
        same_title(title, o.terms);
        const auto parties = take(read_production_asset_private_parties(
            common, o.terms.seller_actor_id, o.terms.buyer_actor_id));
        if (o.terms.price < 0 || parties.payer->id != o.terms.payer_wallet_id ||
            parties.payee->id != o.terms.payee_wallet_id)
          fail("ProductionAssetOriginalPrivatePartiesRequired", o.offer_id);
        if (compare(o.offered_at, previous) < 0 ||
            compare(o.offered_at, now) > 0)
          fail("ProductionAssetOfferMomentMismatch", o.offer_id);
        const std::array<std::string_view, 1> facts{o.offer_id};
        command(common, o.seller_command_source, o.terms.seller_actor_id,
                o.offered_at, facts, {});
        previous = o.offered_at;
      } else if (h.kind == "production_asset_transfer_accepted") {
        const auto &a = acceptance(x.state, entry.first);
        const auto &o = offer(x.state, a.offer_id);
        if (o.terms.asset_ref != id)
          continue;
        same_title(title, o.terms);
        if (a.buyer_actor_id != o.terms.buyer_actor_id ||
            header(common, o.offer_id, "production_asset_transfer_offered") >=
                header(common, a.acceptance_id,
                       "production_asset_transfer_accepted") ||
            compare(a.accepted_at, previous) < 0 ||
            compare(a.accepted_at, now) > 0)
          fail("ProductionAssetBuyerAcceptanceMismatch", a.acceptance_id);
        const ProductionAssetTitleTransfer *applied = nullptr;
        for (const auto &t : x.state.transfers)
          if (t.acceptance_id == a.acceptance_id) {
            if (applied)
              fail("ProductionAssetAcceptanceReused", a.acceptance_id);
            applied = &t;
          }
        if (!applied)
          fail("ProductionAssetAcceptedTransferMissing", a.acceptance_id);
        const std::array<std::string_view, 2> facts{a.acceptance_id,
                                                    applied->transfer_id};
        const std::array<std::string_view, 1> receipts{applied->transfer_id};
        command(common, a.buyer_command_source, a.buyer_actor_id, a.accepted_at,
                facts, receipts);
        previous = a.accepted_at;
      } else if (h.kind == "production_asset_title_transferred") {
        const auto &t = transfer(x.state, entry.first);
        const auto &o = offer(x.state, t.offer_id);
        if (o.terms.asset_ref != id)
          continue;
        const auto &a = acceptance(x.state, t.acceptance_id);
        same_title(title, o.terms);
        if (a.offer_id != o.offer_id ||
            header(common, a.acceptance_id,
                   "production_asset_transfer_accepted") >=
                header(common, t.transfer_id,
                       "production_asset_title_transferred") ||
            compare(t.applied_at, a.accepted_at) ||
            compare(t.applied_at, previous) < 0 ||
            compare(t.applied_at, now) > 0 ||
            t.actual_cash.from_wallet_id != o.terms.payer_wallet_id ||
            t.actual_cash.to_wallet_id != o.terms.payee_wallet_id ||
            t.actual_cash.quantity != o.terms.price)
          fail("ProductionAssetActualTransferMismatch", t.transfer_id);
        title.last_transfer_offer = &o;
        title.last_transfer = &t;
        previous = t.applied_at;
      }
    }
    return {true, title, {}};
  } catch (const Failure &failure) {
    return {false, {}, failure.error};
  }
}
Outcome<ProductionAssetTitleRead>
read_production_asset_title_before_construction(
    const ProductionAssetLifecycleReadContext &x,
    const FacilityConstructionAdoptionContext &construction,
    const MomentOwnerSource &actual_source) {
  try {
    if (&construction.common != &x.installation.common ||
        &construction.assets != &x.installation.production_assets ||
        &construction.facilities != &x.installation.catalog ||
        !same_original_catalog_identity(construction.identity, x.identity) ||
        compare(take(construction.clock.read_current_moment()),
                take(x.clock.read_current_moment())))
      fail("ProductionAssetConstructionContextMismatch",
           actual_source.source_ref.id);
    const auto *adoption =
        take(read_facility_construction_adoption_for_original_dependency(
            construction, actual_source));
    const auto *terms = take(read_facility_construction_terms(
        construction.catalog, adoption->terms_ref));
    auto title = take(read_current_production_asset_title(x, terms->asset_ref));
    title.last_transfer_offer = nullptr;
    title.last_transfer = nullptr;
    const auto &installed = std::get<InstalledPhysicalFacility>(
        title.original_facility->initialization_basis);
    if (compare(installed.installation_evidence.installed_at,
                adoption->started_at) > 0)
      fail("ProductionAssetConstructionBeforeInstallation", terms->asset_ref);
    const auto cut = take(read_actual_common_fact_position(
        construction.common, actual_source.source_ref));
    std::size_t order = 0;
    // 只选已由唯一完整 reader 校验过的实际交割值，不重复发授权或复制规则。
    // 同 Moment 的先后由原 Common 排他 cut 决定，不能只比首尾 Owner。
    for (const auto &entry : construction.common.state().facts.rows()) {
      const auto current = order++;
      if (entry.second.domain != "local_economy" ||
          entry.second.kind != "production_asset_title_transferred")
        continue;
      const auto &applied = transfer(x.state, entry.first);
      const auto &accepted = offer(x.state, applied.offer_id);
      if (accepted.terms.asset_ref != terms->asset_ref)
        continue;
      const auto time_order = compare(applied.applied_at, adoption->started_at);
      if ((current < cut && time_order > 0) ||
          (current >= cut && time_order < 0))
        fail("ProductionAssetConstructionHistoryMomentMismatch",
             applied.transfer_id);
      if (current < cut) {
        title.last_transfer_offer = &accepted;
        title.last_transfer = &applied;
      }
    }
    return {true, title, {}};
  } catch (const Failure &failure) {
    return {false, {}, failure.error};
  }
}
} // namespace hanmo::resident
