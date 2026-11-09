// 商业世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (catalog.private_shop_liquidations) {
  auto own = decode_private_shop_liquidations(
      field(source, "private_shop_liquidations", "state"));
  if (!own.ok)
    return {false, {}, own.error};
  result.private_shop_liquidations =
      std::make_shared<const PrivateShopLiquidationState>(std::move(own.value));
  auto cancelled = decode_task_shop_order_estate_cancellations(
      field(source, "task_shop_order_estate_cancellations", "state"));
  if (!cancelled.ok)
    return {false, {}, cancelled.error};
  if (!result.tasks || !result.tasks->shop_orders)
    fail("TaskShopOrderEstateProfileMismatch",
         "task_shop_order_estate_cancellations");
  auto extended = std::make_shared<TaskState>(*result.tasks);
  extended->shop_orders->estate_cancellations = std::move(cancelled.value);
  result.tasks = std::move(extended);
} else if (source.find("private_shop_liquidations") ||
           source.find("task_shop_order_estate_cancellations"))
  fail("UnexpectedPrivateShopLiquidationState", "state");
