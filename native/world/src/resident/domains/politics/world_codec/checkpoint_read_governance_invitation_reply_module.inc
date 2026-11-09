// 政治检查点恢复分派：沿原 else-if 链读取
// governance_invitation_reply_module；不改变段顺序、所有权或拒绝条件。
else if (section.domain == governance_invitation_reply_module) {
  auto state = make_state<Seed, GovernanceInvitationReplyState>();
  r = visit_governance_invitation_replies_checkpoint(reader, *state);
  seed.governance_invitation_replies = std::move(state);
}