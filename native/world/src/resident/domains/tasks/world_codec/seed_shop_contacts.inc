// 任务世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (catalog.task_shop_orders_enabled) {
  auto parsed =
      decode_task_shop_orders(field(source, "task_shop_orders", "state"));
  if (!parsed.ok)
    return {false, {}, parsed.error};
  auto extended = std::make_shared<TaskState>(*result.tasks);
  extended->shop_orders = std::move(parsed.value);
  result.tasks = std::move(extended);
} else if (source.find("task_shop_orders"))
  fail("UnexpectedTaskShopOrderDomain", "task_shop_orders");
if (catalog.task_shop_order_contacts_enabled) {
  if (!result.tasks || !result.tasks->shop_orders)
    fail("TaskShopOrderContactOriginalRequired", "task_shop_order_contacts");
  auto parsed = decode_task_shop_order_contacts(
      field(source, task_shop_order_contact_module, "state"));
  if (!parsed.ok)
    return {false, {}, parsed.error};
  auto extended = std::make_shared<TaskState>(*result.tasks);
  extended->shop_orders->contacts = std::move(parsed.value);
  result.tasks = std::move(extended);
} else if (source.find(task_shop_order_contact_module))
  fail("UnexpectedTaskShopOrderContactDomain", task_shop_order_contact_module);
