// 共同编解码世界编解码片段：保持原调用位置、字段与首错顺序；仅由
// world_codec.cpp 登记包含。
namespace {
// 首片Bytes兼容口明确受8MiB传输上限约束；它不是发行容量或后台大档保存job。
constexpr std::uint64_t exact_checkpoint_transport_bytes = 8u * 1024u * 1024u;
// 构造在world所属线程同步深复制，复制完成前没有producer；此后私有值从不暴露可写别名。
// 原visitor的写模式不改变业务源。两遍都由同一具名只读租约持有，不借活世界或按hash替代所有权。
class CheckpointWorldSnapshotLease final : public CheckpointSnapshotLease {
  const std::unique_ptr<WorldBundle> world_;
  const CheckpointIdentity identity_;

public:
  explicit CheckpointWorldSnapshotLease(const WorldBundle &source)
      : world_(std::make_unique<WorldBundle>(source)),
        identity_{world_->common.state().identity.world_id,
                  world_->common.revision(), world_->common.tick(),
                  world_->catalog->identity} {
  }
  // 编码 pass 只借这份真正不可写的快照，不借活世界或深复制前根。
  const WorldBundle &original_world() const noexcept { return *world_; }
  const CheckpointIdentity &identity() const override {
    return identity_;
  }
  Status visit_section(const StableId &name, CodecVisitor &c) const override {
    if (c.reading())
      return c.reject("CheckpointSnapshotReadOnly");
    bool found = false;
    auto result = visit_world_checkpoint_sections(
        *world_, [&](const StableId &domain, Integer, auto &&visit) -> Status {
          if (domain != name)
            return common_success();
          if (found)
            return c.reject("DuplicateCheckpointSnapshotDomain", name);
          found = true;
          return visit(c);
        });
    if (!result.ok)
      return result;
    return found ? common_success()
                 : c.reject("UnknownCheckpointSnapshotDomain", name);
  }
};
Outcome<Bytes> encode_exact_world_checkpoint(const WorldBundle &world) {
  try {
    checkpoint_phase_timing::Scope validation_time(checkpoint_phase_timing::Stage::save_validate);
    world_stage_observation::Scope validate_observation(world_stage_observation::Stage::checkpoint_validate);
    auto valid = world.validate();
    validate_observation.stop();
    validation_time.stop();
    if (!valid.ok)
      return {false, {}, valid.error};
    // 未完成项：clone重建索引、common_checkpoint::table/personal表拷贝和Growth候选副本
    // 仍需完整encode-job/scratch计费；不能把此小档兼容口报告为仅chunk内存的全链stream-ready。
    checkpoint_phase_timing::Scope snapshot_time(checkpoint_phase_timing::Stage::save_snapshot);
    world_stage_observation::Scope snapshot_observation(world_stage_observation::Stage::checkpoint_snapshot);
    auto lease = std::make_shared<const CheckpointWorldSnapshotLease>(world);
    snapshot_observation.stop();
    snapshot_time.stop();
    checkpoint_phase_timing::Scope producers_time(checkpoint_phase_timing::Stage::save_producers);
    auto domains = domains_for(*world.catalog);
    std::vector<CheckpointSectionProducer> producers;
    producers.reserve(domains.size());
    for (const auto &domain : domains)
      producers.push_back({domain.name, domain.codec_version, lease});
    producers_time.stop();
    checkpoint_phase_timing::Scope encode_time(checkpoint_phase_timing::Stage::save_encode);
    world_stage_observation::Scope serialize_observation(world_stage_observation::Stage::checkpoint_serialize);
    MilitarySupplyLookupPass supply_lookup(lease->original_world().common.state(),
                                           lease->original_world().military);
    return encode_checkpoint_v2_bytes(lease->identity(), producers, domains,
                                      engineering_checkpoint_capacity_profile(),
                                      exact_checkpoint_transport_bytes);
  } catch (const std::bad_alloc &) {
    return {false, {}, common_failure("CheckpointResourceLimit").error};
  }
}
// 输入Bytes与保留段载荷在同一小档job期间同时存在，候选预算必须先承认这部分实际存量。
Status charge_exact_checkpoint_payloads(CodecDecodeBudget &budget,
                                        const Bytes &bytes,
                                        const CheckpointEnvelope &envelope) {
  auto s = budget.charge_candidate(bytes.size());
  if (!s.ok)
    return s;
  for (const auto &section : envelope.sections) {
    s = budget.charge_candidate(section.payload.size());
    if (!s.ok)
      return s;
  }
  return common_success();
}
} // namespace
// 同一原selector和只写访客分派；const_cast仅适配旧签名，真正reader在入口拒绝。
CheckpointDomainList
select_world_checkpoint_domains(const PreparedCatalog &catalog) {
  return domains_for(catalog);
}
Status visit_captured_world_checkpoint_section(const WorldBundle &world,
                                               const StableId &name,
                                               CodecVisitor &c) {
  if (c.reading())
    return c.reject("CheckpointSnapshotReadOnly");
  bool found = false;
  auto result = visit_world_checkpoint_sections(
      const_cast<WorldBundle &>(world),
      [&](const StableId &domain, Integer, auto &&visit) -> Status {
        if (domain != name)
          return common_success();
        if (found)
          return c.reject("DuplicateCheckpointSnapshotDomain", name);
        found = true;
        return visit(c);
      });
  if (!result.ok)
    return result;
  return found ? common_success()
               : c.reject("UnknownCheckpointSnapshotDomain", name);
}
// 每个section遍历真实typed域，outer先核字节和目录，恢复后仍走全域验证与新epoch发布。
Outcome<Bytes> encode_world_checkpoint(WorldBundle &w) {
  if (w.catalog->exact_time)
    return encode_exact_world_checkpoint(w);
  CheckpointEnvelope envelope;
  envelope.identity = {w.common.state().identity.world_id, w.common.revision(),
                       w.common.tick(), w.catalog->identity};
  auto result = visit_world_checkpoint_sections(
      w, [&](const StableId &name, Integer version, auto &&visit) -> Status {
        CheckpointSection section;
        section.domain = name;
        section.codec_version = version;
        CodecVisitor writer(section.payload);
        auto r = visit(writer);
        if (r.ok)
          r = writer.finish();
        if (r.ok)
          envelope.sections.push_back(std::move(section));
        return r;
      });
  if (!result.ok)
    return {false, {}, result.error};
  // /1 原目录按 selector 的同一顺序逐项核验；访客顺序不是存档顺序。
  // 只交换已完整编码的段，既不重访业务源，也不丢弃未知、重复或错版载荷。
  const auto domains = domains_for(*w.catalog);
  if (envelope.sections.size() != domains.size())
    return {false, {}, common_failure("CheckpointDomainMismatch").error};
  auto position = envelope.sections.begin();
  for (const auto &domain : domains) {
    const auto same_name = [&](const auto &section) {
      return section.domain == domain.name;
    };
    const auto found =
        std::find_if(position, envelope.sections.end(), same_name);
    if (found == envelope.sections.end() ||
        found->codec_version != domain.codec_version ||
        std::find_if(found + 1, envelope.sections.end(), same_name) !=
            envelope.sections.end())
      return {false,
              {},
              common_failure("CheckpointDomainMismatch", domain.name).error};
    std::iter_swap(position, found);
    ++position;
  }
  return encode_checkpoint(envelope);
}
