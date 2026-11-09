// 两本人输入的唯一有序字段；完整准入仍由实际 owner writer 执行。
#pragma once
#include "foundation_asset_commands.hpp"
#include "foundation_asset_fields.hpp"
namespace hanmo::resident::foundation_asset_command_fields {
using namespace exact_fields;
using foundation_asset_fields::begin;
template <class C> Status offer(C &c, OfferFoundationAssets &v) {
  begin(c);
  required_ref(c, "founder_actor_id", v.founder_actor_id);
  c.field("wallet_id", v.wallet_id);
  c.field("cash_quantity", v.cash_quantity);
  c.field("inventory_id", v.inventory_id);
  c.sequence("selected_lots", v.selected_lots, [](auto &out, auto &row) {
    return foundation_asset_resource_fields::selection(out, row);
  });
  return c.object("commitment_window", [&](auto &out) {
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
}
template <class C> Status consent(C &c, ConsentFoundationAssets &v) {
  begin(c);
  c.object("original_offer", [&](auto &out) {
    begin(out);
    return source(out, v.original_offer);
  });
  c.object("actual_intent", [&](auto &out) {
    return foundation_asset_fields::original(out, v.actual_intent);
  });
  schema(c, "expected_terms_revision", v.expected_terms_revision);
  return c.object("actual_content_receipt", [&](auto &out) {
    return foundation_asset_fields::original(out, v.actual_content_receipt);
  });
}
} // namespace hanmo::resident::foundation_asset_command_fields
