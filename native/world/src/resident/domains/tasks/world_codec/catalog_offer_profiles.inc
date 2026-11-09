// 任务世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto shop_orders =
    decode_task_shop_order_profile(scenario, required_capabilities());
if (!shop_orders.ok)
  return {false, {}, shop_orders.error};
result->task_shop_orders_enabled = shop_orders.value;
auto responses = decode_task_offer_response_profile_version(
    scenario, required_capabilities());
if (!responses.ok)
  return {false, {}, responses.error};
result->task_offer_responses = responses.value != 0;
result->task_offer_response_version = responses.value;
auto handoffs =
    decode_task_message_handoff_version(scenario, required_capabilities());
if (!handoffs.ok)
  return {false, {}, handoffs.error};
result->task_message_handoffs_enabled = handoffs.value != 0;
result->task_message_handoff_version = handoffs.value;
auto static_assets =
    decode_task_static_asset_profile(scenario, required_capabilities());
if (!static_assets.ok)
  return {false, {}, static_assets.error};
result->task_static_assets_enabled = static_assets.value;
auto notices =
    decode_task_message_notice_catalog(scenario, required_capabilities());
if (!notices.ok)
  return {false, {}, notices.error};
if (notices.value)
  result->task_message_notices =
      checkpoint_make_shared<const TaskMessageNoticeCatalog>(
          std::move(*notices.value));
