// 商业世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (catalog.private_shop_command_results) {
  auto parsed = decode_private_shop_command_results_state(
      field(source, private_shop_command_results_module, "state"));
  if (!parsed.ok)
    return {false, {}, parsed.error};
  result.private_shop_command_results =
      std::make_shared<const PrivateShopCommandResultsState>(
          std::move(parsed.value));
} else if (source.find(private_shop_command_results_module))
  fail("UnexpectedPrivateShopCommandResultsState",
       private_shop_command_results_module);
