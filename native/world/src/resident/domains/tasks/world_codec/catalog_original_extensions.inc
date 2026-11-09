// 任务世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto tasks = decode_task_catalog(data);
if (!tasks.ok)
  return {false, {}, tasks.error};
result->tasks =
    checkpoint_make_shared<const TaskCatalog>(std::move(tasks.value));
if (result->task_personal_advances) {
  auto valid = validate_task_personal_advance_catalog(
      *result->task_personal_advances, *result->tasks);
  if (!valid.ok)
    return {false, {}, valid.error};
}
auto task_extensions =
    decode_task_extension_profile(scenario, required_capabilities());
if (!task_extensions.ok)
  return {false, {}, task_extensions.error};
if (task_extensions.value) {
  auto decoded = decode_task_extension_catalog(
      field(scenario, "task_extensions", "scenario"), *result->tasks,
      *result->interaction);
  if (!decoded.ok)
    return {false, {}, decoded.error};
  result->task_extensions = checkpoint_make_shared<const TaskExtensionCatalog>(
      std::move(decoded.value));
}
