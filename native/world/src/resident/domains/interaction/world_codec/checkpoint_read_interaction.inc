// 交互检查点恢复分派：沿原 else-if 链读取
// interaction；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "interaction") {
  auto physical = native_interaction_physical_version(catalog);
  if (!physical.ok)
    return {false, {}, std::move(physical.error)};
  if (section.codec_version != static_cast<std::uint32_t>(physical.value))
    return {false,
            {},
            common_failure("InteractionResponseSelectionMismatch").error};
  r = visit_native_interaction_checkpoint(reader, *i, catalog);
}