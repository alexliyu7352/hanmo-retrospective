// 任务世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto shop_contacts =
    decode_task_shop_order_contact_profile(scenario, required_capabilities());
if (!shop_contacts.ok)
  return {false, {}, shop_contacts.error};
result->task_shop_order_contacts_enabled = shop_contacts.value;
if (shop_contacts.value && (!result->task_shop_orders_enabled ||
                            !result->private_shops || !result->family_life))
  return {false,
          {},
          common_failure("TaskShopOrderContactDependencyRequired").error};
auto message_returns =
    decode_task_message_return_profile(scenario, required_capabilities());
if (!message_returns.ok)
  return {false, {}, message_returns.error};
if (message_returns.value) {
  if (result->dynamic_profile != DynamicProfile::local_society_v1 ||
      !result->family_life)
    return {false,
            {},
            common_failure("TaskMessageReturnOriginalSourcesRequired").error};
  auto parsed = decode_task_message_return_catalog(
      field(scenario, "task_message_returns", "scenario"), *result->tasks,
      *result->interaction);
  if (!parsed.ok)
    return {false, {}, parsed.error};
  auto pair = validate_task_message_return_extension_pair(
      parsed.value, result->task_extensions.get());
  if (!pair.ok)
    return {false, {}, pair.error};
  result->task_message_returns =
      checkpoint_make_shared<const TaskMessageReturnCatalog>(
          std::move(parsed.value));
}
auto personal_messages =
    decode_task_personal_message_profile(scenario, required_capabilities());
if (!personal_messages.ok)
  return {false, {}, personal_messages.error};
if (personal_messages.value) {
  if (result->dynamic_profile != DynamicProfile::local_society_v1 ||
      !result->family_life || !result->known_personal_dialogues)
    return {false,
            {},
            common_failure("TaskPersonalMessageOriginalSourcesRequired").error};
  auto parsed = decode_task_personal_message_catalog(
      field(field(scenario, "dynamic_world", "scenario"),
            task_personal_message_module, "dynamic_world"),
      *result->tasks, *result->known_personal_dialogues, *result->interaction);
  if (!parsed.ok)
    return {false, {}, parsed.error};
  auto pair = validate_task_personal_message_extension_pair(
      parsed.value, result->task_extensions.get());
  if (!pair.ok)
    return {false, {}, pair.error};
  result->task_personal_messages =
      checkpoint_make_shared<const TaskPersonalMessageCatalog>(
          std::move(parsed.value));
}
auto personal_reports =
    decode_task_personal_report_profile(scenario, required_capabilities());
if (!personal_reports.ok)
  return {false, {}, personal_reports.error};
if (personal_reports.value) {
  if (!result->task_personal_messages || !result->known_personal_dialogues)
    return {false,
            {},
            common_failure("TaskPersonalReportOriginalSourcesRequired").error};
  auto parsed = decode_task_personal_report_catalog(
      field(field(scenario, "dynamic_world", "scenario"),
            task_personal_report_module, "dynamic_world"),
      *result->tasks, *result->task_personal_messages,
      *result->known_personal_dialogues, *result->interaction);
  if (!parsed.ok)
    return {false, {}, parsed.error};
  auto pair = validate_task_personal_report_extension_pair(
      parsed.value, result->task_extensions.get());
  if (!pair.ok)
    return {false, {}, pair.error};
  result->task_personal_reports =
      checkpoint_make_shared<const TaskPersonalReportCatalog>(
          std::move(parsed.value));
}
auto personal_acceptances =
    decode_task_personal_acceptance_profile(scenario, required_capabilities());
if (!personal_acceptances.ok)
  return {false, {}, personal_acceptances.error};
if (personal_acceptances.value) {
  if (!result->task_personal_messages || !result->known_personal_dialogues)
    return {
        false,
        {},
        common_failure("TaskPersonalAcceptanceOriginalSourcesRequired").error};
  auto parsed = decode_task_personal_acceptance_catalog(
      field(field(scenario, "dynamic_world", "scenario"),
            task_personal_acceptance_module, "dynamic_world"),
      *result->tasks, *result->task_personal_messages,
      *result->known_personal_dialogues, *result->interaction);
  if (!parsed.ok)
    return {false, {}, parsed.error};
  auto pair = validate_task_personal_acceptance_extension_pair(
      parsed.value, result->task_extensions.get());
  if (!pair.ok)
    return {false, {}, pair.error};
  result->task_personal_acceptances =
      checkpoint_make_shared<const TaskPersonalAcceptanceCatalog>(
          std::move(parsed.value));
  auto task_alias = checkpoint_make_shared<TaskCatalog>(*result->tasks);
  task_alias->personal_acceptances = result->task_personal_acceptances;
  result->tasks = std::move(task_alias);
}
auto personal_cancellations = decode_task_personal_cancellation_profile(
    scenario, required_capabilities());
if (!personal_cancellations.ok)
  return {false, {}, personal_cancellations.error};
if (personal_cancellations.value) {
  if (!result->task_personal_messages || !result->known_personal_dialogues)
    return {false,
            {},
            common_failure("TaskPersonalCancellationOriginalSourcesRequired")
                .error};
  auto parsed = decode_task_personal_cancellation_catalog(
      field(field(scenario, "dynamic_world", "scenario"),
            task_personal_cancellation_module, "dynamic_world"),
      *result->tasks, *result->task_personal_messages,
      *result->known_personal_dialogues, *result->interaction);
  if (!parsed.ok)
    return {false, {}, parsed.error};
  auto pair = validate_task_personal_cancellation_extension_pair(
      parsed.value, result->task_extensions.get());
  if (!pair.ok)
    return {false, {}, pair.error};
  result->task_personal_cancellations =
      checkpoint_make_shared<const TaskPersonalCancellationCatalog>(
          std::move(parsed.value));
}
