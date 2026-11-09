// 交互检查点恢复分派：沿原 else-if 链读取
// known_personal_dialogues；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "known_personal_dialogues") {
  i->known_personal_dialogues.emplace();
  r = visit_known_personal_dialogues_checkpoint(reader,
                                                *i->known_personal_dialogues);
}