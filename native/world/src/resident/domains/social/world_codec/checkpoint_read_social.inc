// 社交检查点恢复分派：沿原 else-if 链读取
// social；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "social") r =
    visit_native_social_checkpoint(reader, *s, catalog);
