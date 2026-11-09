// 交互世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto invitations =
    decode_party_invitation_profile(scenario, required_capabilities());
if (!invitations.ok)
  return {false, {}, invitations.error};
result->party_invitations_enabled = invitations.value;
auto recruitment =
    decode_party_recruitment_consent_profile(scenario, required_capabilities());
if (!recruitment.ok)
  return {false, {}, recruitment.error};
auto visit_messages =
    decode_visit_message_profile_version(scenario, required_capabilities());
if (!visit_messages.ok)
  return {false, {}, visit_messages.error};
auto message_receipts = decode_visit_message_receipt_profile_version(
    scenario, required_capabilities());
if (!message_receipts.ok)
  return {false, {}, message_receipts.error};
result->visit_message_receipts_enabled = message_receipts.value != 0;
auto visit_reception =
    decode_visit_reception_profile(scenario, required_capabilities());
if (!visit_reception.ok)
  return {false, {}, visit_reception.error};
