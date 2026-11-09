// 军事检查点恢复分派：沿原 else-if 链读取
// population_military；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "population_military") r =
    visit_population_military_checkpoint(reader, *population,
                                         population_version(catalog),
                                         siege_enabled(catalog));
