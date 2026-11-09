// 共同编解码世界编解码片段：保持原调用位置、字段与首错顺序；仅由
// world_codec.cpp 登记包含。
Outcome<std::vector<CheckpointDomain>>
checkpoint_domains_from_identity(const CheckpointIdentity &identity) {
  return selected_checkpoint_domains(identity);
}
