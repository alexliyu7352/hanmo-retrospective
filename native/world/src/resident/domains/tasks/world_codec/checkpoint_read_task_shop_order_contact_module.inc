// 任务检查点恢复分派：沿原 else-if 链读取
// task_shop_order_contact_module；不改变段顺序、所有权或拒绝条件。
else if (section.domain == task_shop_order_contact_module) {
  if (!tasks->shop_orders)
    return {false,
            {},
            common_failure("TaskShopOrderContactOriginalRequired").error};
  tasks->shop_orders->contacts.emplace();
  r = visit_task_shop_order_contacts_checkpoint(reader,
                                                *tasks->shop_orders->contacts);
}