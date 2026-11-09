// 商业世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
// 日租独立段持有全载荷；共同JSON只留三种精确头，孤实体/孤头由原layout双向覆盖拒绝。
bool daily_rent_fact_kind(const StableId &kind) {
  return kind == private_shop_rent_assessed_kind ||
         kind == private_shop_rent_paid_kind ||
         kind == private_shop_rent_resumed_kind;
}
// 身后执行双段的共同事实逐原件投影；旧Shop宽前缀仅排除这四个具名kind，再由本口严格核域。
bool shop_estate_fact_kind(const StableId &kind) {
  return kind == private_shop_liquidation_mandate_kind ||
         kind == private_shop_liquidation_acceptance_kind ||
         kind == private_shop_liquidation_revocation_kind ||
         kind == private_shop_liquidation_receipt_kind;
}
