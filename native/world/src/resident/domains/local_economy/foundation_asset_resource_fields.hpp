// B1 冷资源证据的唯一字段序。帧、Owner、reservation 仍委托 Common 原
// JSON/binary codec；这里不定义另一套钱包/库存格式或历史执行算法。
#pragma once
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "foundation_asset_resource_effects.hpp"
#include "resident/kernel/exact_struct_codec.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "resident/storage/common_projection.hpp"
namespace hanmo::resident::foundation_asset_resource_fields {
struct Layout {
  bool typed;
  const CommonResourceCodecContext &resources;
};
// 严格读取本首次根的顺序；旧 Common 帧保持它们自己的原字段顺序。
template <class C> Status begin(C &c) {
  if constexpr (std::is_same_v<std::decay_t<C>, ActorFoundationJsonVisitor>)
    return c.ordered_fields();
  return c.status();
}
template <class C> Status owner(C &c, Owner &v) {
  if constexpr (std::is_same_v<std::decay_t<C>, CodecVisitor>)
    return common_checkpoint::owner(c, v);
  else
    return c.foreign_record(
        v,
        [](const BoundaryValue &raw,
           ScopedCheckpointJsonDecode &budget) -> Outcome<Owner> {
          try {
            if (ScopedCheckpointJsonDecode::current() != &budget)
              return {false,
                      {},
                      common_failure("CheckpointJsonContextMismatch").error};
            budget.default_values<Owner>();
            return {true,
                    boundary::decode_owner(raw, "foundation_asset/owner"),
                    {}};
          } catch (const boundary::DecodeFailure &failure) {
            return {false, {}, failure.error};
          }
        },
        emit_common_owner_fields);
}
template <class C> Status wallet(C &c, WalletView &v, const Layout &l) {
  if constexpr (std::is_same_v<std::decay_t<C>, CodecVisitor>)
    return common_checkpoint::wallet(c, v);
  else
    return c.foreign_record(
        v,
        [&](const BoundaryValue &raw, ScopedCheckpointJsonDecode &budget) {
          return decode_common_wallet_frame(raw, "foundation_asset/wallet",
                                            l.typed, l.resources, budget);
        },
        [&](ProjectionWriter &out, const WalletView &row) {
          return emit_common_wallet_frame_fields(out, row, l.typed,
                                                 l.resources);
        });
}
template <class C> Status inventory(C &c, InventoryView &v, const Layout &l) {
  if constexpr (std::is_same_v<std::decay_t<C>, CodecVisitor>)
    return common_checkpoint::inventory(c, v);
  else
    return c.foreign_record(
        v,
        [&](const BoundaryValue &raw, ScopedCheckpointJsonDecode &budget) {
          return decode_common_inventory_frame(
              raw, "foundation_asset/inventory", l.resources, budget);
        },
        [&](ProjectionWriter &out, const InventoryView &row) {
          return emit_common_inventory_frame_fields(out, row, l.typed,
                                                    l.resources);
        });
}
// 本首次业务明确只支持原 kind3 typed 布局；未选真实 Common 能力直接拒绝。
template <class C>
Status reservation(C &c, const char *name, ReservationRef &v, const Layout &l) {
  return c.object(name, [&](auto &out) {
    if constexpr (std::is_same_v<std::decay_t<C>, CodecVisitor>) {
      static const StableId field = "reservation";
      return common_checkpoint::reservation(out, v, field);
    } else {
      return out.foreign_record(
          v,
          [&](const BoundaryValue &raw,
              ScopedCheckpointJsonDecode &budget) -> Outcome<ReservationRef> {
            try {
              if (ScopedCheckpointJsonDecode::current() != &budget)
                return {false,
                        {},
                        common_failure("CheckpointJsonContextMismatch").error};
              if (raw.as<StableId>())
                return {false,
                        {},
                        common_failure("TypedReservationRequired").error};
              budget.default_values<ReservationRef>();
              return {true,
                      boundary::decode_reservation_ref(
                          raw, "foundation_asset/reservation",
                          l.resources.domain_activity_reservation_refs_enabled),
                      {}};
            } catch (const boundary::DecodeFailure &failure) {
              return {false, {}, failure.error};
            }
          },
          [&](ProjectionWriter &writer, const ReservationRef &row) {
            return emit_common_reservation_fields(
                writer, row, true,
                l.resources.domain_activity_reservation_refs_enabled);
          });
    }
  });
}
template <class C> Status selection(C &c, CargoSelection &v) {
  begin(c);
  exact_fields::required_ref(c, "inventory_id", v.inventory_id);
  exact_fields::required_ref(c, "lot_id", v.lot_id);
  c.field("quantity", v.quantity);
  if (c.status().ok && v.quantity <= 0)
    return c.reject("FoundationAssetPositiveQuantityRequired", v.lot_id);
  return c.status();
}
template <class C> Status request(C &c, WalletDelta &v, const Layout &) {
  begin(c);
  c.field("wallet_id", v.wallet_id);
  c.field("delta", v.delta);
  return c.field("require_spendable", v.require_spendable);
}
template <class C> Status request(C &c, SetReservation &v, const Layout &l) {
  begin(c);
  c.field("wallet_id", v.wallet_id);
  reservation(c, "action_id", v.action_id, l);
  c.field("amount", v.amount);
  return c.field("erase", v.erase);
}
template <class C>
Status request(C &c, ReserveSelectedLots &v, const Layout &l) {
  begin(c);
  c.field("inventory_id", v.inventory_id);
  c.sequence("selections", v.selections,
             [](auto &out, auto &row) { return selection(out, row); });
  c.sequence("owners", v.owners,
             [](auto &out, auto &row) { return owner(out, row); });
  return reservation(c, "reservation", v.reservation, l);
}
template <class C>
Status request(C &c, TransferSelectedLots &v, const Layout &l) {
  begin(c);
  c.field("source_id", v.source_id);
  c.field("destination_id", v.destination_id);
  c.sequence("selections", v.selections,
             [](auto &out, auto &row) { return selection(out, row); });
  c.sequence("owners", v.owners,
             [](auto &out, auto &row) { return owner(out, row); });
  reservation(c, "reservation", v.reservation, l);
  return c.optional("new_owner", v.new_owner,
                    [](auto &out, auto &row) { return owner(out, row); });
}
template <class C>
Status inventory_frame(C &c, ResourceInventoryExecutionDraft &v,
                       const Layout &l) {
  begin(c);
  c.field("inventory_id", v.inventory_id);
  c.optional("before", v.before,
             [&](auto &out, auto &row) { return inventory(out, row, l); });
  return c.object("after",
                  [&](auto &out) { return inventory(out, v.after, l); });
}
// 仅允许真正精确批预约/整批转入的原映射 tag，不接受 FIFO 或消耗替身。
template <class C> Status flow(C &c, ResourceLotFlowDraft &v, bool transfer) {
  begin(c);
  Integer kind = static_cast<Integer>(v.kind);
  c.field("kind", kind);
  if (!c.status().ok)
    return c.status();
  using Kind = ResourceLotFlowDraft::Kind;
  if (transfer ? kind != Integer(Kind::selected_transfer_whole)
               : kind != Integer(Kind::selected_reserve_whole) &&
                     kind != Integer(Kind::selected_reserve_split))
    return c.reject("FoundationAssetOriginalSelectedFlowRequired");
  if (c.reading())
    v.kind = static_cast<Kind>(kind);
  c.field("source_inventory_id", v.source_inventory_id);
  c.field("source_lot_id", v.source_lot_id);
  c.field("destination_inventory_id", v.destination_inventory_id);
  c.field("destination_lot_id", v.destination_lot_id);
  c.field("item_id", v.item_id);
  return c.field("quantity", v.quantity);
}
template <class C, class T> Status wallet_effect(C &c, T &v, const Layout &l) {
  begin(c);
  c.object("request", [&](auto &out) { return request(out, v.request, l); });
  c.object("before", [&](auto &out) { return wallet(out, v.before, l); });
  return c.object("after", [&](auto &out) { return wallet(out, v.after, l); });
}
template <class C, class T>
Status selected_effect(C &c, T &v, const Layout &l, bool transfer) {
  begin(c);
  c.object("request", [&](auto &out) { return request(out, v.request, l); });
  c.sequence("inventories", v.inventories, [&](auto &out, auto &row) {
    return inventory_frame(out, row, l);
  });
  c.sequence("flows", v.flows,
             [&](auto &out, auto &row) { return flow(out, row, transfer); });
  c.field("next_instance_before", v.next_instance_before);
  return c.field("next_instance_after", v.next_instance_after);
}
template <class C>
Status preparation(C &c, FoundationAssetPreparationEffects &v,
                   const Layout &l) {
  begin(c);
  c.optional("cash", v.cash,
             [&](auto &out, auto &row) { return wallet_effect(out, row, l); });
  return c.optional("goods", v.goods, [&](auto &out, auto &row) {
    return selected_effect(out, row, l, false);
  });
}
template <class C>
Status fulfillment(C &c, FoundationAssetFulfillmentEffects &v,
                   const Layout &l) {
  begin(c);
  c.optional("cash", v.cash, [&](auto &out, auto &row) {
    begin(out);
    out.object("release",
               [&](auto &leaf) { return wallet_effect(leaf, row.release, l); });
    out.object("debit",
               [&](auto &leaf) { return wallet_effect(leaf, row.debit, l); });
    return out.object("credit", [&](auto &leaf) {
      return wallet_effect(leaf, row.credit, l);
    });
  });
  return c.optional("goods", v.goods, [&](auto &out, auto &row) {
    return selected_effect(out, row, l, true);
  });
}
} // namespace hanmo::resident::foundation_asset_resource_fields
