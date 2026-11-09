// 交互检查点恢复分派：沿原 else-if 链读取 visit_messages,
// visit_reception_claims；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "visit_messages") {
  i->visit_messages.emplace();
  r = visit_visit_messages_checkpoint(reader, *i->visit_messages);
  if (r.ok && i->visit_messages->version != section.codec_version)
    r = common_failure("InteractionSectionPayloadVersionMismatch",
                       section.domain);
}
else if (section.domain == "visit_reception_claims") {
  i->visit_reception_claims.emplace();
  r = visit_visit_reception_checkpoint(reader, *i->visit_reception_claims);
}