// 共同编解码世界编解码片段：保持原调用位置、字段与首错顺序；仅由
// world_codec.cpp 登记包含。
Outcome<InitialWorldSeed>
decode_world_checkpoint(const Bytes &bytes, const PreparedCatalog &catalog) {
  auto simultaneous = validate_personal_simultaneous_outcome_modules(
      catalog.identity.module_versions,
      catalog.personal &&
          !catalog.personal->simultaneous_outcome_objectives.empty());
  if (!simultaneous.ok)
    return {false, {}, simultaneous.error};
  auto decoded =
      catalog.exact_time
          ? decode_checkpoint_v2_bytes(
                bytes, catalog.identity, domains_for(catalog),
                engineering_checkpoint_capacity_profile())
          : decode_checkpoint(bytes, catalog.identity, domains_for(catalog));
  if (!decoded.ok)
    return {false, {}, decoded.error};
  CodecDecodeBudget budget(engineering_checkpoint_capacity_profile());
  if (decoded.value.container_version == 2) {
    auto charged =
        charge_exact_checkpoint_payloads(budget, bytes, decoded.value);
    if (!charged.ok)
      return {false, {}, charged.error};
  }
  return WorldCheckpointDecoder::decode<InitialWorldSeed>(
      decoded.value, catalog, budget, [&](const auto &section) {
        return decoded.value.container_version == 2
                   ? CodecVisitor(section.payload,
                                  engineering_checkpoint_capacity_profile(),
                                  section.domain, budget)
                   : CodecVisitor(section.payload);
      });
}
namespace {
// 摘要覆盖真正送入typed
// reader的每个字节，防检查后换包/ABA；只接严格顺序单段读取。
class DecodedSectionSource final : public CheckpointReadSource {
public:
  DecodedSectionSource(CheckpointReadSource &source,
                       const CheckpointSectionIndex &index)
      : source_(source), index_(index) {
  }
  std::uint64_t size() const override {
    return index_.bytes;
  }
  Status read(std::uint64_t at, std::uint8_t *out, std::size_t n) override {
    if (at != read_ || at > index_.bytes || n > index_.bytes - at)
      return common_failure("CheckpointSourceChanged");
    auto r = source_.read(index_.offset + at, out, n);
    if (!r.ok)
      return r;
    for (std::size_t p = 0; p < n;) {
      const auto amount = std::min<std::size_t>(64, n - p);
      hash_.process(out + p, out + p + amount);
      p += amount;
    }
    read_ += n;
    return common_success();
  }
  Status finish() {
    if (read_ != index_.bytes)
      return common_failure("CheckpointSourceChanged");
    hash_.finish();
    CheckpointDigest actual{};
    hash_.get_hash_bytes(actual.begin(), actual.end());
    return actual == index_.sha256 ? common_success()
                                   : common_failure("CheckpointSourceChanged");
  }

private:
  CheckpointReadSource &source_;
  const CheckpointSectionIndex &index_;
  picosha2::hash256_one_by_one hash_;
  std::uint64_t read_ = 0;
};
} // namespace
// 有界源先核完整物理目录，再以同 job 构造私有候选；不发布世界。
// 失败直接交既有 WorldRestoreCandidate 边界捕获，不能在失败 job 中再造空 seed。
CheckpointJobResult<DecodedWorldSeedOwner> decode_world_checkpoint_stream(
    CheckpointReadSource &source, const PreparedCatalog &catalog,
    const CapacityProfile &capacity, CodecDecodeBudget &budget,
    CheckpointJobBudget &job, const CheckpointDigest *expected_full_digest) {
  try {
    auto profile = job.check_profile(capacity);
    if (!profile.ok)
      throw checkpoint_job_detail::FailureException{std::move(profile.error)};
    auto simultaneous = validate_personal_simultaneous_outcome_modules(
        catalog.identity.module_versions,
        catalog.personal &&
            !catalog.personal->simultaneous_outcome_objectives.empty());
    if (!simultaneous.ok)
      throw checkpoint_job_detail::FailureException{
          job.capture_failure(std::move(simultaneous.error))};
    auto index = inspect_checkpoint_v2_stream(source, domains_for(catalog),
                                              capacity, job);
    if (!index.ok)
      throw checkpoint_job_detail::FailureException{std::move(index.error)};
    if (expected_full_digest &&
        index.value.value().receipt.full_checkpoint_sha256 !=
            *expected_full_digest)
      throw checkpoint_job_detail::FailureException{
          job.capture_boundary_failure("CheckpointSourceChanged", {},
                                       ErrorKind::rule)};
    const auto &a = index.value.value().identity.catalog;
    const auto &b = catalog.identity;
    if (std::tie(a.content_id, a.content_version, a.artifact_sha256,
                 a.content_digest, a.ruleset_id, a.schema_sha256,
                 a.pack_set_digest, a.module_versions) !=
        std::tie(b.content_id, b.content_version, b.artifact_sha256,
                 b.content_digest, b.ruleset_id, b.schema_sha256,
                 b.pack_set_digest, b.module_versions))
      throw checkpoint_job_detail::FailureException{
          job.capture_boundary_failure("CheckpointCatalogMismatch", {},
                                       ErrorKind::rule)};
    auto memory = checkpoint_memory_resource(job, CheckpointJobUse::candidate);
    if (!memory.ok)
      throw checkpoint_job_detail::FailureException{std::move(memory.error)};
    ScopedCheckpointMemory scope(memory.value);
    auto scratch = job.reserve(CheckpointJobUse::scratch,
                               1024 + sizeof(DecodedSectionSource));
    if (!scratch.ok)
      throw checkpoint_job_detail::FailureException{std::move(scratch.error)};
    std::optional<DecodedSectionSource> active;
    auto decoded = WorldCheckpointDecoder::decode<DecodedWorldSeedOwner>(
        index.value.value(), catalog, budget, [&](const auto &section) {
          if (active) {
            auto verified = active->finish();
            if (!verified.ok)
              throw checkpoint_job_detail::FailureException{
                  job.capture_failure(std::move(verified.error))};
          }
          active.emplace(source, section);
          return CodecVisitor(*active, 0, section.bytes, capacity,
                              section.domain, budget);
        });
    if (!decoded.ok)
      throw checkpoint_job_detail::FailureException{
          job.capture_failure(std::move(decoded.error))};
    if (active) {
      auto verified = active->finish();
      if (!verified.ok)
        throw checkpoint_job_detail::FailureException{
            job.capture_failure(std::move(verified.error))};
    }
    return {true, std::move(decoded.value), {}};
  } catch (checkpoint_job_detail::FailureException &) {
    // 保持原首错 lease；外层候选空值不分配，已负责同一种异常的运输。
    throw;
  } catch (const std::bad_alloc &) {
    throw checkpoint_job_detail::FailureException{
        job.capture_fixed_failure(CheckpointJobFailureCode::allocation_failed)};
  } catch (const std::length_error &) {
    throw checkpoint_job_detail::FailureException{
        job.capture_fixed_failure(CheckpointJobFailureCode::size_overflow)};
  }
}
