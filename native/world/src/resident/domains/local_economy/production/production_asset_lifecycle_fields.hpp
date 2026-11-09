// 首次 production_asset_lifecycle/1
// 唯一字段序；旧经济/设施布局不变，不补造旧来源。
#pragma once
#include "facility_construction_fields.hpp"
#include "production_asset_lifecycle.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/facility_claims/facility_claims_codec_fields.hpp"

namespace hanmo::resident::production_asset_lifecycle_fields {
using namespace exact_fields;
using facility_construction_fields::begin;
using facility_construction_fields::original;
using facility_construction_fields::rule;
template <class C> Status terms(C &c, ProductionAssetTransferTerms &v) {
  begin(c);
  required_ref(c, "asset_ref", v.asset_ref);
  c.object("previous_title_source",
           [&](auto &out) { return original(out, v.previous_title_source); });
  required_ref(c, "seller_actor_id", v.seller_actor_id);
  required_ref(c, "buyer_actor_id", v.buyer_actor_id);
  required_ref(c, "payer_wallet_id", v.payer_wallet_id);
  required_ref(c, "payee_wallet_id", v.payee_wallet_id);
  c.field("price", v.price);
  if (c.status().ok &&
      (v.price < 0 || v.seller_actor_id == v.buyer_actor_id ||
       v.payer_wallet_id == v.payee_wallet_id))
    return c.reject("ProductionAssetInvalidTransferTerms", v.asset_ref);
  return c.status();
}
template <class C> Status offer(C &c, ProductionAssetTransferOffer &v) {
  begin(c);
  required_ref(c, "offer_id", v.offer_id);
  c.object("terms", [&](auto &out) { return terms(out, v.terms); });
  c.object("offered_at", [&](auto &out) { return moment(out, v.offered_at); });
  return c.object("seller_command_source", [&](auto &out) {
    return source(out, v.seller_command_source);
  });
}
template <class C>
Status acceptance(C &c, ProductionAssetTransferAcceptance &v) {
  begin(c);
  required_ref(c, "acceptance_id", v.acceptance_id);
  required_ref(c, "offer_id", v.offer_id);
  required_ref(c, "buyer_actor_id", v.buyer_actor_id);
  c.object("accepted_at",
           [&](auto &out) { return moment(out, v.accepted_at); });
  return c.object("buyer_command_source", [&](auto &out) {
    return source(out, v.buyer_command_source);
  });
}
template <class C> Status cash(C &c, ProductionAssetTransferCash &v) {
  begin(c);
  required_ref(c, "from_wallet_id", v.from_wallet_id);
  required_ref(c, "to_wallet_id", v.to_wallet_id);
  c.field("quantity", v.quantity);
  if (c.status().ok &&
      (v.quantity < 0 || v.from_wallet_id == v.to_wallet_id))
    return c.reject("ProductionAssetInvalidTransferCash", v.from_wallet_id);
  return c.status();
}
template <class C> Status transfer(C &c, ProductionAssetTitleTransfer &v) {
  begin(c);
  required_ref(c, "transfer_id", v.transfer_id);
  required_ref(c, "offer_id", v.offer_id);
  required_ref(c, "acceptance_id", v.acceptance_id);
  c.object("applied_at", [&](auto &out) { return moment(out, v.applied_at); });
  return c.object("actual_cash",
                  [&](auto &out) { return cash(out, v.actual_cash); });
}
template <class C> Status consent(C &c, ProductionAssetConstructionConsent &v) {
  begin(c);
  required_ref(c, "consent_id", v.consent_id);
  required_ref(c, "asset_ref", v.asset_ref);
  required_ref(c, "owner_actor_id", v.owner_actor_id);
  required_ref(c, "builder_actor_id", v.builder_actor_id);
  c.object("construction_terms_ref",
           [&](auto &out) { return rule(out, v.construction_terms_ref); });
  c.object("title_source",
           [&](auto &out) { return original(out, v.title_source); });
  c.object("usage_source", [&](auto &out) {
    return facility_fields::source_ref(out, v.usage_source);
  });
  c.object("consented_at",
           [&](auto &out) { return moment(out, v.consented_at); });
  return c.object("owner_command_source", [&](auto &out) {
    return source(out, v.owner_command_source);
  });
}
// 原 tracked 四表保持实际追加顺序；重复 ID 必须硬拒，不允许覆盖旧同意。
template <class C, class T, class Visit, class Key>
Status rows(C &c, const char *name, CheckpointVector<T> &values, Visit visit,
            Key key) {
  std::size_t index = 0;
  return c.sequence(name, values, [&](auto &out, auto &row) {
    visit(out, row);
    if (out.status().ok)
      for (std::size_t prior = 0; prior < index; ++prior)
        if (key(values[prior]) == key(row))
          return out.reject("DuplicateProductionAssetLifecycleSource",
                            key(row));
    ++index;
    return out.status();
  });
}
template <class C> Status state(C &c, ProductionAssetLifecycleState &v) {
  begin(c);
  c.field("schema_version", v.schema_version);
  if (c.status().ok && v.schema_version != 1)
    return c.reject("UnsupportedProductionAssetLifecycleVersion");
  rows(
      c, "offers", v.offers,
      [](auto &out, auto &row) { return offer(out, row); },
      [](const auto &row) -> const StableId & { return row.offer_id; });
  rows(
      c, "acceptances", v.acceptances,
      [](auto &out, auto &row) { return acceptance(out, row); },
      [](const auto &row) -> const StableId & { return row.acceptance_id; });
  rows(
      c, "transfers", v.transfers,
      [](auto &out, auto &row) { return transfer(out, row); },
      [](const auto &row) -> const StableId & { return row.transfer_id; });
  return rows(
      c, "construction_consents", v.construction_consents,
      [](auto &out, auto &row) { return consent(out, row); },
      [](const auto &row) -> const StableId & { return row.consent_id; });
}
} // namespace hanmo::resident::production_asset_lifecycle_fields
