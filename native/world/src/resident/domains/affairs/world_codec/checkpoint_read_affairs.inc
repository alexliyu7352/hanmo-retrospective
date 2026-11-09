// 事务检查点恢复分派：沿原 else-if 链读取
// affairs；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "affairs") r = visit_affairs_checkpoint(
    reader, *affairs, catalog.affairs->obligation_needs,
    fiscal_needs_enabled(catalog), cohort_wage_needs_enabled(catalog));
