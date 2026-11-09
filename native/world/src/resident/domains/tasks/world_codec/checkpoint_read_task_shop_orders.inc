// 任务检查点恢复分派：沿原 else-if 链读取
// task_shop_orders；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "task_shop_orders") {
  tasks->shop_orders.emplace();
  r = visit_task_shop_orders_checkpoint(reader, *tasks->shop_orders);
}