// 交互检查点恢复分派：沿原 else-if 链读取
// visit_message_receipt_module；不改变段顺序、所有权或拒绝条件。
else if (section.domain == visit_message_receipt_module) {
  i->visit_message_receipts.emplace();
  r = visit_visit_message_receipts_checkpoint(reader,
                                              *i->visit_message_receipts);
}