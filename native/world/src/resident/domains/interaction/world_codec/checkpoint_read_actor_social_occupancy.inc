// 交互检查点恢复分派：沿原 else-if 链读取
// actor_social_occupancy；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "actor_social_occupancy") {
  auto version =
      selected_native_actor_social_occupancy_version(catalog.identity);
  if (!version.ok)
    return {false, {}, std::move(version.error)};
  if (!catalog.actor_social_occupancy_selected ||
      section.codec_version != version.value || i->social_occupancy)
    return {
        false,
        {},
        common_failure("CheckpointActorSocialOccupancyProfileMismatch").error};
  i->social_occupancy.emplace();
  r = visit_actor_social_occupancy_checkpoint(reader, *i->social_occupancy);
  if (r.ok && i->social_occupancy->version != version.value)
    r = common_failure("ActorSocialOccupancyModuleMismatch");
}