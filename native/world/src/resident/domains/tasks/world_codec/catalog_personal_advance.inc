// 任务世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto advances =
    decode_task_personal_advance_profile(scenario, required_capabilities());
if (!advances.ok)
  return {false, {}, advances.error};
if (advances.value)
  result->task_personal_advances =
      checkpoint_make_shared<const TaskPersonalAdvanceCatalog>(
          std::move(*advances.value));
