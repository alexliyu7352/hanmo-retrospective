// 行旅世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto transport = decode_native_transport_json(source, catalog, result,
                                              owned_budget, owned_job);
if (!transport.ok)
  return {false, {}, std::move(transport.error)};
