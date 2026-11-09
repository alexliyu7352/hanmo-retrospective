// 成长世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。 卡片只扩显式新growth格式，逐一保留旧3/4/5/6的区域生活与逐课组合。
template <class DomainList> DomainList card_domains(DomainList domains) {
  for (auto &domain : domains)
    if (domain.name == "growth") {
      if (domain.codec_version < 3 || domain.codec_version > 6)
        boundary::fail("UnsupportedCardsCheckpointProfile",
                       "checkpoint/growth");
      domain.codec_version += 4;
    }
  return domains;
}
// 显式邀学对应原Growth八种格式逐项加8，旧版本序列不增加存在位。
template <class DomainList> DomainList teaching_domains(DomainList domains) {
  for (auto &domain : domains)
    if (domain.name == "growth") {
      if (domain.codec_version < 3 || domain.codec_version > 10)
        boundary::fail("UnsupportedTeachingCheckpointProfile",
                       "checkpoint/growth");
      domain.codec_version += 8;
    }
  return domains;
}
// 普通授课显式对应旧Growth3..18逐项加16；两类教学来源独立开关。
template <class DomainList>
DomainList study_teaching_domains(DomainList domains) {
  for (auto &domain : domains)
    if (domain.name == "growth") {
      if (domain.codec_version < 3 || domain.codec_version > 18)
        boundary::fail("UnsupportedStudyTeachingCheckpointProfile",
                       "checkpoint/growth");
      domain.codec_version += 16;
    }
  return domains;
}
