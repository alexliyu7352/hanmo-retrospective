// foundation_assets/1 首次四表唯一字段表；JSON/binary/retention 共用此序。
#pragma once
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "foundation_asset_resource_fields.hpp"
#include "foundation_asset_sources.hpp"
namespace hanmo::resident::foundation_asset_fields {
using namespace exact_fields;
using foundation_asset_resource_fields::begin;
using Layout = foundation_asset_resource_fields::Layout;
template <class C> Status original(C &c, MomentOwnerSource &v) {
  begin(c);
  c.object("source_ref", [&](auto &out) {
    begin(out);
    return source(out, v.source_ref);
  });
  return schema(c, "source_schema", v.source_schema);
}
template <class C> Status offered_lot(C &c, FoundationOfferedLot &v) {
  begin(c);
  c.object("selection", [&](auto &out) {
    return foundation_asset_resource_fields::selection(out, v.selection);
  });
  required_ref(c, "item_id", v.item_id);
  optional_ref(c, "item_instance_id", v.item_instance_id);
  return c.optional(
      "properties_ref", v.properties_ref, [](auto &out, auto &row) {
        begin(out);
        return required_ref(out, "properties_id", row.properties_id);
      });
}
template <class C> Status terms(C &c, FoundationAssetOfferTerms &v) {
  begin(c);
  required_ref(c, "owner_actor_id", v.owner_actor_id);
  required_ref(c, "founder_actor_id", v.founder_actor_id);
  c.field("wallet_id", v.wallet_id);
  c.field("cash_quantity", v.cash_quantity);
  c.field("inventory_id", v.inventory_id);
  c.sequence("lots", v.lots,
             [](auto &out, auto &row) { return offered_lot(out, row); });
  c.object("commitment_window", [&](auto &out) {
    begin(out);
    out.object("from", [&](auto &leaf) {
      begin(leaf);
      return moment(leaf, v.commitment_window.from);
    });
    return out.object("to", [&](auto &leaf) {
      begin(leaf);
      return moment(leaf, v.commitment_window.to);
    });
  });
  if (c.status().ok &&
      (v.cash_quantity < 0 || (!v.cash_quantity && v.lots.empty()) ||
       (v.cash_quantity == 0) != v.wallet_id.empty() ||
       v.lots.empty() != v.inventory_id.empty()))
    return c.reject("FoundationAssetOriginalOfferRequired");
  return c.status();
}
template <class C> Status offer(C &c, FoundationAssetOffer &v) {
  begin(c);
  required_ref(c, "id", v.id);
  c.object("terms", [&](auto &out) { return terms(out, v.terms); });
  c.object("offered_at", [&](auto &out) {
    begin(out);
    return moment(out, v.offered_at);
  });
  return c.object("owner_command_source", [&](auto &out) {
    begin(out);
    return source(out, v.owner_command_source);
  });
}
template <class C> Status consent(C &c, FoundationAssetConsent &v) {
  begin(c);
  required_ref(c, "id", v.id);
  required_ref(c, "offer_id", v.offer_id);
  c.object("intent_source",
           [&](auto &out) { return original(out, v.intent_source); });
  schema(c, "intent_terms_revision", v.intent_terms_revision);
  c.object("actual_content_receipt",
           [&](auto &out) { return original(out, v.actual_content_receipt); });
  c.object("consented_at", [&](auto &out) {
    begin(out);
    return moment(out, v.consented_at);
  });
  return c.object("owner_command_source", [&](auto &out) {
    begin(out);
    return source(out, v.owner_command_source);
  });
}
template <class C>
Status prepared(C &c, FoundationAssetPreparation &v, const Layout &l) {
  begin(c);
  required_ref(c, "id", v.id);
  required_ref(c, "consent_id", v.consent_id);
  c.object("prepared_at", [&](auto &out) {
    begin(out);
    return moment(out, v.prepared_at);
  });
  c.sequence("actual_lots", v.actual_lots, [](auto &out, auto &row) {
    begin(out);
    required_ref(out, "original_lot_id", row.original_lot_id);
    return required_ref(out, "reserved_lot_id", row.reserved_lot_id);
  });
  return c.object("actual_resources", [&](auto &out) {
    return foundation_asset_resource_fields::preparation(out,
                                                         v.actual_resources, l);
  });
}
template <class C>
Status fulfilled(C &c, FoundationAssetFulfillment &v, const Layout &l) {
  begin(c);
  required_ref(c, "id", v.id);
  required_ref(c, "preparation_id", v.preparation_id);
  required_ref(c, "organization_id", v.organization_id);
  c.field("destination_wallet_id", v.destination_wallet_id);
  c.field("destination_inventory_id", v.destination_inventory_id);
  c.object("fulfilled_at", [&](auto &out) {
    begin(out);
    return moment(out, v.fulfilled_at);
  });
  c.sequence("actual_lots", v.actual_lots, [](auto &out, auto &row) {
    begin(out);
    required_ref(out, "reserved_lot_id", row.reserved_lot_id);
    return required_ref(out, "delivered_lot_id", row.delivered_lot_id);
  });
  return c.object("actual_resources", [&](auto &out) {
    return foundation_asset_resource_fields::fulfillment(out,
                                                         v.actual_resources, l);
  });
}
// 重复源 ID 不得靠今天表顺序遮盖；全根对 Common 的反向覆盖另由原 reader 验。
template <class C, class V, class Fn>
Status rows(C &c, const char *name, V &v, Fn fn) {
  std::size_t index = 0;
  return c.sequence(name, v, [&](auto &out, auto &row) {
    fn(out, row);
    if (out.status().ok)
      for (std::size_t prior = 0; prior < index; ++prior)
        if (v[prior].id == row.id)
          return out.reject("DuplicateFoundationAssetSource", row.id);
    ++index;
    return out.status();
  });
}
template <class C>
Status state(C &c, FoundationAssetSourcesState &v, const Layout &l) {
  begin(c);
  if (!l.typed || !l.resources.domain_activity_reservation_refs_enabled)
    return c.reject("FoundationAssetTypedDomainReservationRequired");
  c.field("schema_version", v.schema_version);
  if (c.status().ok && v.schema_version != 1)
    return c.reject("UnsupportedFoundationAssetsVersion");
  rows(c, "offers", v.offers,
       [](auto &out, auto &row) { return offer(out, row); });
  rows(c, "consents", v.consents,
       [](auto &out, auto &row) { return consent(out, row); });
  rows(c, "preparations", v.preparations,
       [&](auto &out, auto &row) { return prepared(out, row, l); });
  return rows(c, "fulfillments", v.fulfillments,
              [&](auto &out, auto &row) { return fulfilled(out, row, l); });
}
} // namespace hanmo::resident::foundation_asset_fields
