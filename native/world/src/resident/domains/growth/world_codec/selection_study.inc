// 成长世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
// 逐课政策仅提升实际改变的成长格式；旧目录和各既有组合的版本保持原样。
template <class DomainList> DomainList study_domains(DomainList domains) {
  for (auto &domain : domains)
    if (domain.name == "growth")
      domain.codec_version = domain.codec_version == 4 ? 6 : 5;
  return domains;
}
