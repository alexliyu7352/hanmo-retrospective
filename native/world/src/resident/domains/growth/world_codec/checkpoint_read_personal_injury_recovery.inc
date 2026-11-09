// 成长检查点恢复分派：沿原 else-if 链读取
// personal_injury_recovery；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "personal_injury_recovery") r =
    visit_growth_recovery_checkpoint(reader, *recovery);
