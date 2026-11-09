// 政治检查点恢复分派：沿原 else-if 链读取
// political_supply_adoptions；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "political_supply_adoptions") {
  auto state = make_state<Seed, PoliticalSupplyAdoptionState>();
  r = visit_political_supply_adoptions_checkpoint(reader, *state);
  seed.political_supply_adoptions = std::move(state);
}