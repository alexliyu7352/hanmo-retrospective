// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
// 报价保持只读；卖方本人采用同一实际产权报价才写不可变 offer。
#include "production_asset_transfer.hpp"
#include "production_asset_construction_candidate.hpp"
#include "production_asset_transfer_cash.hpp"
#include "resident/kernel/command_types.hpp"
#include "resident/kernel/common_command_sources.hpp"
#include "resident/kernel/original_catalog_identity.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/storage/money_transfer_recipe.hpp"
#include <stdexcept>

namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
template <class T> T take(Outcome<T> result) {
  if (!result.ok)
    throw Failure{std::move(result.error)};
  return std::move(result.value);
}
// 相同世界借用和同 job 新字段所有者缺一不可，不能向另一个根借一份额度。
void candidate(const ProductionAssetLifecycleWriteContext &x) {
  const auto current = current_checkpoint_memory();
  const auto &owner = x.owned_fields.resource();
  if (&x.common != &x.read.installation.common || &x.state != &x.read.state ||
      !current || !owner || !current->shares_job(*owner))
    fail("ProductionAssetActualCandidateMemoryRequired");
  const auto same_allocator = [&](const auto &rows) {
    const auto allocator = rows.get_allocator();
    const auto &resource = allocator.resource();
    return resource && current->shares_job(*resource);
  };
  if (!same_allocator(x.state.offers) || !same_allocator(x.state.acceptances) ||
      !same_allocator(x.state.transfers) ||
      !same_allocator(x.state.construction_consents))
    fail("ProductionAssetActualCandidateAllocatorRequired");
}
// 原精确容量新壳只接受逐条 typed move；WorldMoment 的 BigNat 仍由实际
// allocator 收费。若中途抛出，整私有候选必须弃置，不尝试恢复半搬前像。
template <class T>
void append_original_row(CheckpointVector<T> &rows, T &&value) {
  CheckpointVector<T> next(rows.get_allocator());
  if (rows.size() == rows.max_size())
    fail("ProductionAssetLifecycleCapacityExceeded");
  next.reserve(rows.size() + 1);
  for (auto &row : rows)
    next.emplace_back(std::move(row));
  next.emplace_back(std::move(value));
  rows.swap(next);
}
// 从刚校原头借其唯一原名字，不创建“当前产权”新 source。
SourceRefBuildView title_source(const ProductionAssetTitleRead &title) {
  if (title.last_transfer)
    return {"local_economy", "production_asset_title_transferred",
            title.last_transfer->transfer_id};
  const auto &installed = std::get<InstalledPhysicalFacility>(
      title.original_facility->initialization_basis);
  return {"local_economy", "economic_facility_installed",
          installed.installation_evidence.installation_id};
}
// 所有字节已按原 fresh-range 规则先预留，构造不复用未知旧 capacity。
StableId fresh(std::string_view value) {


  StableId result(value.data(), value.size());
  return result;
}
} // namespace

Outcome<ProductionAssetTransferQuoteRead>
quote_production_asset_transfer(const ProductionAssetLifecycleReadContext &x,
                                const StableId &seller, const StableId &asset,
                                const StableId &buyer, Integer price) {
  try {
    if (price < 0)
      fail("ProductionAssetInvalidPrice", asset);
    auto title = take(read_current_production_asset_title(x, asset));
    if (!production_asset_title_owned_by(title, Owner::Kind::actor, seller))
      fail("ProductionAssetPersonalSellerRequired", asset);
    auto parties = take(read_production_asset_private_parties(
        x.installation.common, seller, buyer));
    return {true,
            {title, parties, &x.installation.common.state().identity, price},
            {}};
  } catch (const Failure &failure) {
    return {false, {}, failure.error};
  }
}
Outcome<ProductionAssetPendingCommand>
accept_production_asset_transfer_quote_in_candidate(
    ProductionAssetLifecycleWriteContext &x, const StableId &seller,
    const StableId &asset, const StableId &buyer, Integer price,
    const MomentOwnerSource &expected_title) {
  try {
    candidate(x);
    const auto quote = take(
        quote_production_asset_transfer(x.read, seller, asset, buyer, price));
    if (!production_asset_title_source_equal(quote.title, expected_title))
      fail("ProductionAssetTransferTitleChanged", asset);
    const auto now = take(x.read.clock.read_current_moment());
    auto id = take(x.common.next_owned_id("asset_offer"));
    const auto source = title_source(quote.title);


    (x.owned_fields).grow(0);
    ProductionAssetTransferOffer offer{
        fresh(id.id()),
        {fresh(asset),
         {{fresh(source.domain), fresh(source.kind), fresh(source.id)}, 1},
         fresh(seller),
         fresh(buyer),
         fresh(quote.parties.payer->id),
         fresh(quote.parties.payee->id),
         price},
        now,
        {}};
    const auto index = x.state.offers.size();
    append_original_row(x.state.offers, std::move(offer));
    take(x.common.append_fact_borrowed(
        id.id(), "production_asset_transfer_offered", "local_economy"));
    return {true,
            {ProductionAssetPendingCommand::Kind::seller_offer, index,
             std::nullopt, std::nullopt},
            {}};
  } catch (const Failure &failure) {
    return {false, {}, failure.error};
  }
}
Outcome<ProductionAssetPendingCommand>
accept_production_asset_transfer_offer_in_candidate(
    ProductionAssetLifecycleWriteContext &x, const StableId &buyer,
    const SourceRef &accepted_offer) {
  try {
    candidate(x);
    if (accepted_offer.domain != "local_economy" ||
        accepted_offer.kind != "production_asset_transfer_offered")
      fail("ProductionAssetOriginalAcceptedOfferRequired", accepted_offer.id);
    const ProductionAssetTransferOffer *offer = nullptr;
    for (const auto &row : x.state.offers)
      if (row.offer_id == accepted_offer.id) {
        if (offer)
          fail("DuplicateProductionAssetLifecycleSource", accepted_offer.id);
        offer = &row;
      }
    if (!offer || offer->terms.buyer_actor_id != buyer)
      fail("ProductionAssetOriginalBuyerRequired", accepted_offer.id);
    const auto &terms = offer->terms;
    const auto title =
        take(read_current_production_asset_title(x.read, terms.asset_ref));
    if (!production_asset_title_owned_by(title, Owner::Kind::actor,
                                         terms.seller_actor_id) ||
        !production_asset_title_source_equal(title,
                                             terms.previous_title_source))
      fail("ProductionAssetTransferTitleChanged", terms.asset_ref);
    const auto parties = take(read_production_asset_private_parties(
        x.common, terms.seller_actor_id, buyer));
    if (parties.payer->id != terms.payer_wallet_id ||
        parties.payee->id != terms.payee_wallet_id)
      fail("ProductionAssetTransferPrivateWalletChanged", offer->offer_id);
    if (!x.common.resource_execution_capture_identity())
      fail("ProductionAssetActualTransferCaptureRequired", offer->offer_id);
    // 当前真正源读已核卖方成功命令；买方不预写同意，不借第三方行动。
    const auto now = take(x.read.clock.read_current_moment());
    auto acceptance_id = take(x.common.next_owned_id("asset_acceptance"));
    auto transfer_id = take(x.common.next_owned_id("asset_transfer"));


    (x.owned_fields).grow(0);
    ProductionAssetTransferAcceptance accepted{fresh(acceptance_id.id()),
                                               fresh(offer->offer_id),
                                               fresh(buyer),
                                               now,
                                               {}};
    ProductionAssetTitleTransfer transferred{fresh(transfer_id.id()),
                                             fresh(offer->offer_id),
                                             fresh(acceptance_id.id()),
                                             now,
                                             {fresh(terms.payer_wallet_id),
                                              fresh(terms.payee_wallet_id),
                                              terms.price}};
    // 本临时 recipe 的新壳和两个字符串按原范围先收费；Common capture
    // 自己持有真实前后像，不借这份临时 lease 冒其长期预算。


    const auto recipe_memory = CheckpointMemoryCharge(0);
    ResourceRecipe recipe;
    recipe.operations.reserve(2);
    take(resource_recipe::append_money_transfer(
        recipe, terms.payer_wallet_id, terms.payee_wallet_id, terms.price,
        resource_recipe::MoneyTransferPolicy::spendable));
    const auto capture_index = x.common.resource_execution_drafts().size();
    take(x.common.apply_resources(recipe));
    const auto &drafts = x.common.resource_execution_drafts();
    if (drafts.size() != capture_index + 1)
      fail("ProductionAssetActualTransferCaptureRequired", offer->offer_id);
    take(validate_production_asset_transfer_cash_capture(
        x.common, terms, transferred, drafts.back(), x.read.clock));
    const auto accepted_index = x.state.acceptances.size();
    const auto transferred_index = x.state.transfers.size();
    append_original_row(x.state.acceptances, std::move(accepted));
    append_original_row(x.state.transfers, std::move(transferred));
    take(x.common.append_fact_borrowed(acceptance_id.id(),
                                       "production_asset_transfer_accepted",
                                       "local_economy"));
    take(x.common.append_fact_borrowed(transfer_id.id(),
                                       "production_asset_title_transferred",
                                       "local_economy"));
    return {true,
            {ProductionAssetPendingCommand::Kind::buyer_transfer,
             transferred_index, accepted_index, capture_index},
            {}};
  } catch (const Failure &failure) {
    return {false, {}, failure.error};
  }
}
Outcome<const ProductionAssetTransferOffer *>
read_pending_production_asset_offer(
    const ProductionAssetLifecycleWriteContext &x,
    const ProductionAssetPendingCommand &pending) {
  try {
    candidate(x);
    if (pending.kind != ProductionAssetPendingCommand::Kind::seller_offer ||
        pending.acceptance_index || pending.resource_execution_index ||
        pending.row_index >= x.state.offers.size())
      fail("ProductionAssetPendingOfferRequired");
    const auto &offer = x.state.offers[pending.row_index];
    const auto *head = x.common.state().facts.find(offer.offer_id);
    if (!head || head->id != offer.offer_id ||
        head->domain != "local_economy" ||
        head->kind != "production_asset_transfer_offered" ||
        !offer.seller_command_source.domain.empty() ||
        !offer.seller_command_source.kind.empty() ||
        !offer.seller_command_source.id.empty())
      fail("ProductionAssetPendingOfferMismatch", offer.offer_id);
    return {true, &offer, {}};
  } catch (const Failure &failure) {
    return {false, nullptr, failure.error};
  }
}
// 只有原 S0 已缓存的成功本人结果可封口；传进一个同形 CommitResult 副本也拒。
Status bind_production_asset_offer_command_result_in_candidate(
    ProductionAssetLifecycleWriteContext &x,
    const ProductionAssetPendingCommand &pending, const CommitResult &receipt) {
  try {
    const auto *original =
        take(read_pending_production_asset_offer(x, pending));
    const auto actual = take(read_common_command_source(
        x.common, std::string_view(receipt.result_id)));
    const auto *effects =
        receipt.details
            ? std::get_if<LocalEconomyEffects>(&receipt.details->data)
            : nullptr;
    if (actual.result != &receipt ||
        receipt.kind != "accept_property_transfer" ||
        actual.history->actor_id != original->terms.seller_actor_id ||
        receipt.tick != original->offered_at.half_day_tick || !effects ||
        effects->fact_ids.size() != 1 ||
        effects->fact_ids.front() != original->offer_id ||
        !effects->receipt_ids.empty() ||
        take(compare_world_moments(original->offered_at,
                                   take(x.read.clock.read_current_moment()))))
      fail("ProductionAssetActualSellerSealRequired", original->offer_id);

    const SourceRefBuildView source{"common", "command_result",
                                    receipt.result_id};

    (x.owned_fields).grow(0);
    x.state.offers[pending.row_index].seller_command_source =
        SourceRef{fresh(source.domain), fresh(source.kind), fresh(source.id)};
    take(
        read_current_production_asset_title(x.read, original->terms.asset_ref));
    return common_success();
  } catch (const Failure &failure) {
    return {false, {}, failure.error};
  }
}

Outcome<ProductionAssetPendingConstructionConsent>
adopt_production_asset_construction_consent_in_candidate(
    ProductionAssetLifecycleWriteContext &x,
    const FacilityConstructionAdoptionContext &construction,
    const StableId &actor, const EconomicRuleRef &terms_ref,
    const MomentOwnerSource &expected_title,
    const FacilitySourceRef &actual_usage) {
  try {
    candidate(x);
    if (&construction.common != &x.common ||
        &construction.assets != &x.read.installation.production_assets ||
        &construction.facilities != &x.read.installation.catalog ||
        !same_original_catalog_identity(construction.identity, x.read.identity))
      fail("ProductionAssetConstructionContextMismatch", terms_ref.id);
    take(validate_facility_construction_catalog(
        construction.catalog, construction.identity,
        construction.common_catalog, construction.assets,
        construction.facilities));
    const auto *terms =
        take(read_facility_construction_terms(construction.catalog, terms_ref));
    const auto title =
        take(read_current_production_asset_title(x.read, terms->asset_ref));
    if (!production_asset_title_owned_by(title, Owner::Kind::actor, actor) ||
        !production_asset_title_source_equal(title, expected_title) ||
        !title.initial_owner ||
        title.initial_owner->kind != Owner::Kind::actor ||
        title.initial_owner->id != actor)
      fail("ProductionAssetPersonalConstructionAuthorityRequired",
           terms->asset_ref);
    const auto usage = take(read_production_asset_initial_usage_history(
        x.read, actual_usage, *title.initial_owner));
    const auto now = take(x.read.clock.read_current_moment());
    const auto other_now = take(construction.clock.read_current_moment());
    if (take(compare_world_moments(now, other_now)) || !usage.opened_at ||
        take(compare_world_moments(*usage.opened_at, now)) > 0 ||
        usage.ended_at || usage.facility != title.original_facility ||
        usage.facility->place_ref != terms->place_id)
      fail("ProductionAssetConstructionUsageUnavailable", terms->asset_ref);
    auto id = take(x.common.next_owned_id("asset_construction_consent"));
    const auto original_title = title_source(title);
    const SourceRefBuildView original_usage{actual_usage.source_ref.domain,
                                            actual_usage.source_ref.kind,
                                            actual_usage.source_ref.id};


    (x.owned_fields).grow(0);
    ProductionAssetConstructionConsent consent{
        fresh(id.id()),
        fresh(terms->asset_ref),
        fresh(actor),
        fresh(actor),
        {fresh(terms->terms_ref.id), terms->terms_ref.version,
         fresh(terms->terms_ref.catalog_digest)},
        {{fresh(original_title.domain), fresh(original_title.kind),
          fresh(original_title.id)},
         1},
        {{fresh(original_usage.domain), fresh(original_usage.kind),
          fresh(original_usage.id)},
         actual_usage.source_schema},
        now,
        {}};
    const auto index = x.state.construction_consents.size();
    append_original_row(x.state.construction_consents, std::move(consent));
    take(x.common.append_fact_borrowed(
        id.id(), "production_asset_construction_consented", "local_economy"));
    return {true, {index}, {}};
  } catch (const Failure &failure) {
    return {false, {}, failure.error};
  }
}
Outcome<const ProductionAssetConstructionConsent *>
read_pending_production_asset_construction_consent(
    const ProductionAssetLifecycleWriteContext &x,
    const ProductionAssetPendingConstructionConsent &pending) {
  try {
    candidate(x);
    if (pending.row_index >= x.state.construction_consents.size())
      fail("ProductionAssetPendingConstructionConsentRequired");
    const auto &row = x.state.construction_consents[pending.row_index];
    const auto *header = x.common.state().facts.find(row.consent_id);
    if (!header || header->id != row.consent_id ||
        header->domain != "local_economy" ||
        header->kind != "production_asset_construction_consented" ||
        !row.owner_command_source.domain.empty() ||
        !row.owner_command_source.kind.empty() ||
        !row.owner_command_source.id.empty())
      fail("ProductionAssetPendingConstructionConsentRequired", row.consent_id);
    return {true, &row, {}};
  } catch (const Failure &failure) {
    return {false, nullptr, failure.error};
  }
}
Status bind_production_asset_construction_command_result_in_candidate(
    ProductionAssetLifecycleWriteContext &x,
    const FacilityConstructionAdoptionContext &construction,
    const ProductionAssetPendingConstructionConsent &pending,
    const MomentOwnerSource &actual_start, const CommitResult &result) {
  try {
    const auto *row =
        take(read_pending_production_asset_construction_consent(x, pending));
    const auto original =
        take(read_common_command_source(x.common, result.result_id));
    if (original.result != &result || result.kind != "plan_facility_capacity" ||
        original.history->actor_id != row->owner_actor_id)
      fail("ProductionAssetActualConstructionCommandRequired", row->consent_id);
    const auto *started =
        take(read_facility_construction_adoption_for_original_dependency(
            construction, actual_start));
    if (started->authority_sources.size() != 1 ||
        started->authority_sources.front().source_schema != 1 ||
        started->authority_sources.front().source_ref.domain !=
            "local_economy" ||
        started->authority_sources.front().source_ref.kind !=
            "production_asset_construction_consented" ||
        started->authority_sources.front().source_ref.id != row->consent_id)
      fail("ProductionAssetConstructionCommandResultsMismatch",
           row->consent_id);
    const SourceRefBuildView source{"common", "command_result",
                                    result.result_id};


    (x.owned_fields).grow(0);
    x.state.construction_consents[pending.row_index].owner_command_source = {
        fresh(source.domain), fresh(source.kind), fresh(source.id)};
    take(read_production_asset_original_construction_consent(
        x.read, construction, actual_start));
    return common_success();
  } catch (const Failure &failure) {
    return {false, {}, failure.error};
  }
}
} // namespace hanmo::resident
