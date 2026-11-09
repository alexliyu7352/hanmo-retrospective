// 首次施工成本的唯一资源字段表；原钱包/库存及typed预约仍委托Common原布局。
#pragma once
#include "../foundation_asset_resource_fields.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "facility_construction_resource_effects.hpp"
namespace hanmo::resident::facility_construction_resource_fields {
using Layout = foundation_asset_resource_fields::Layout;
using foundation_asset_resource_fields::begin;
// 原Consume四成员保持原顺序，不加入归还、预付款或自由结果标记。
template <class C>
Status request(C &c, ConsumeSelectedLots &v, const Layout &layout) {
  begin(c);
  c.field("inventory_id", v.inventory_id);
  c.sequence("selections", v.selections, [](auto &child, auto &row) {
    return foundation_asset_resource_fields::selection(child, row);
  });
  c.sequence("owners", v.owners, [](auto &child, auto &row) {
    return foundation_asset_resource_fields::owner(child, row);
  });
  return foundation_asset_resource_fields::reservation(c, "reservation",
                                                       v.reservation, layout);
}
// 只接受真实selected_consume流；其他原语不能借同样数量冒材料消耗。
template <class C> Status flow(C &c, ResourceLotFlowDraft &v) {
  begin(c);
  Integer kind = static_cast<Integer>(v.kind);
  c.field("kind", kind);
  if (!c.status().ok)
    return c.status();
  if (kind !=
      static_cast<Integer>(ResourceLotFlowDraft::Kind::selected_consume))
    return c.reject("ConstructionOriginalConsumeFlowRequired");
  if (c.reading())
    v.kind = ResourceLotFlowDraft::Kind::selected_consume;
  c.field("source_inventory_id", v.source_inventory_id);
  c.field("source_lot_id", v.source_lot_id);
  c.field("destination_inventory_id", v.destination_inventory_id);
  c.field("destination_lot_id", v.destination_lot_id);
  c.field("item_id", v.item_id);
  return c.field("quantity", v.quantity);
}
template <class C>
Status consumption(C &c, ConstructionSelectedConsumptionEffect &v,
                   const Layout &layout) {
  begin(c);
  c.object("request",
           [&](auto &child) { return request(child, v.request, layout); });
  c.sequence("inventories", v.inventories, [&](auto &child, auto &row) {
    return foundation_asset_resource_fields::inventory_frame(child, row,
                                                             layout);
  });
  c.sequence("flows", v.flows,
             [](auto &child, auto &row) { return flow(child, row); });
  c.field("next_instance_before", v.next_instance_before);
  return c.field("next_instance_after", v.next_instance_after);
}
template <class C>
Status effects(C &c, ConstructionStartCostEffects &v, const Layout &layout) {
  begin(c);
  c.sequence("materials", v.materials, [&](auto &child, auto &row) {
    return consumption(child, row, layout);
  });
  return c.optional("payment", v.payment, [&](auto &child, auto &row) {
    begin(child);
    child.object("debit", [&](auto &leaf) {
      return foundation_asset_resource_fields::wallet_effect(leaf, row.debit,
                                                             layout);
    });
    return child.object("credit", [&](auto &leaf) {
      return foundation_asset_resource_fields::wallet_effect(leaf, row.credit,
                                                             layout);
    });
  });
}
} // namespace hanmo::resident::facility_construction_resource_fields
