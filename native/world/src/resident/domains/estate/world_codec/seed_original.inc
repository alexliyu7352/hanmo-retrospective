// 庄园世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto e = [&]() {
  ScopedCheckpointJsonDecode original(CheckpointWorldJsonSection::estate);
  return catalog.estate_employment
             ? decode_estate_seed(source, result.estate_employment.get())
             : decode_estate_seed(source);
}();
if (!e.ok)
  return {false, {}, e.error};
result.estate = std::make_shared<const EstateState>(std::move(e.value));
