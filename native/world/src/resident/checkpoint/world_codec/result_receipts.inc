// 共同编解码世界编解码片段：保持原调用位置、字段与首错顺序；仅由
// world_codec.cpp 登记包含。
Outcome<std::shared_ptr<const CommitResult>>
ResidentResultCodec::decode(const BoundaryValue &v) const {
  return decode_resident_receipt(v);
}
// 缓存结果只借用原持有者，同一字段writer同时支撑JSON与shape。
Status ResidentResultCodec::emit(ProjectionWriter &out, const StableId &name,
                                 const CommitResult &value) const {
  return emit_resident_receipt(out, name, value);
}
BoundaryValue ResidentResultCodec::encode(const CommitResult &value) const {
  BoundaryValue output;
  BoundaryProjectionWriter writer(output);
  auto status = emit(writer, "", value);
  if (status.ok)
    status = writer.finish();
  if (!status.ok)
    boundary::fail(status.error.original.code, status.error.original.path);
  return output;
}
Status
ResidentResultCodec::visit(CodecVisitor &c,
                           std::shared_ptr<const CommitResult> &v) const {
  return visit_resident_receipt(c, v);
}
