// 任务检查点恢复分派：沿原 else-if 链读取
// task_shop_order_estate_cancellations；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "task_shop_order_estate_cancellations") {
  if (!tasks->shop_orders)
    return {
        false, {}, common_failure("TaskShopOrderEstateProfileMismatch").error};
  tasks->shop_orders->estate_cancellations.emplace();
  r = visit_task_shop_order_estate_cancellations_checkpoint(
      reader, *tasks->shop_orders->estate_cancellations);
}