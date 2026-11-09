// 庄园检查点恢复分派：沿原 else-if 链读取
// estate；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "estate") r = visit_estate_checkpoint(reader, *e);
