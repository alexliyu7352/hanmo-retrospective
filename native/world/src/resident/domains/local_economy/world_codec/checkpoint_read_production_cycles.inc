// 地方经济检查点恢复分派：沿原 else-if 链读取
// production_cycles；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "production_cycles") {
  if (!catalog.production_cycles || section.codec_version != 3 ||
      seed.production_cycles)
    return {false,
            {},
            common_failure("ProductionCyclesCheckpointProfileMismatch", {},
                           ErrorKind::internal)
                .error};
  auto state = make_state<Seed, EconomicWorkSourcesState>();
  r = visit_economic_work_sources(
      reader, *state, EconomicWorkSourceSchema::production_calendar3);
  seed.production_cycles = std::move(state);
}