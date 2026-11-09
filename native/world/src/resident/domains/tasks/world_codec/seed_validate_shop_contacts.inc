// 任务世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
{
  CommonStore view(result.common, catalog.common);
  auto checked = validate_task_shop_order_contact_json_payloads(
      view,
      result.tasks && result.tasks->shop_orders &&
              result.tasks->shop_orders->contacts
          ? &*result.tasks->shop_orders->contacts
          : nullptr,
      source);
  if (!checked.ok)
    return {false, {}, checked.error};
}
