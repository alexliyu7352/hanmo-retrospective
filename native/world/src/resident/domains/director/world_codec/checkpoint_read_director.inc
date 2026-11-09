// 决策检查点恢复分派：沿原 else-if 链读取
// director；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "director") r =
    visit_native_director_checkpoint(reader, *director, catalog);
