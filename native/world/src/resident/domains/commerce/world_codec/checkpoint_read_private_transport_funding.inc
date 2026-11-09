// 商业检查点恢复分派：沿原 else-if 链读取
// private_transport_funding；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "private_transport_funding") {
  if (!catalog.private_transport_funding_selected ||
      section.codec_version != 1 || seed.private_transport_funding)
    return {false,
            {},
            common_failure("PrivateTransportFundingCheckpointProfileMismatch")
                .error};
  auto state = make_state<Seed, PrivateTransportFundingState>();
  r = visit_private_transport_funding_checkpoint(reader, *state);
  seed.private_transport_funding = std::move(state);
}