// 军事世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。 新能力用独立域表；无适配器的1.1继续原本16段及版本。
const std::vector<CheckpointDomain> population_checkpoint_domains = []() {
  auto domains = dynamic_checkpoint_domains;
  for (auto &domain : domains)
    if (domain.name == "local_economy")
      domain.codec_version = 2;
  domains.push_back({"population_military", 4});
  return domains;
}();
