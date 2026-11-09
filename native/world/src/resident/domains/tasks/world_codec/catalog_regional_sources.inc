// 任务世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (result->task_message_notices) {
  auto valid = validate_task_message_notice_catalog(
      *result->task_message_notices, *result->tasks, *result->common);
  if (!valid.ok)
    return {false, {}, valid.error};
}
if (result->regional_life) {
  const auto &regional = field(field(scenario, "dynamic_world", "scenario"),
                               "regional_life", "dynamic_world");
  if (const auto *outcomes = regional.find("outcome_topics")) {
    auto extended =
        checkpoint_make_shared<InteractionCatalog>(*result->interaction);
    auto valid =
        decode_regional_outcome_catalog(*outcomes, *extended, *result->tasks);
    if (!valid.ok)
      return {false, {}, valid.error};
    result->interaction = std::move(extended);
  }
}
