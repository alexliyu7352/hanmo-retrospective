// 共同编解码检查点恢复分派：沿原 else-if 链读取 profile_deliveries,
// world_timeline, action_elapsed；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "profile_deliveries") {
  auto outbox = make_state<Seed, ProfileDeliveryOutbox>();
  r = visit_profile_delivery_outbox(reader, *outbox);
  seed.profile_deliveries = std::move(outbox);
}
else if (section.domain == "world_timeline") r =
    visit_world_timeline2(reader, *exact_timeline);
else if (section.domain == "action_elapsed") r =
    visit_action_elapsed2(reader, *exact_elapsed);
