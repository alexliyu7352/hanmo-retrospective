// 共同编解码世界编解码片段：保持原调用位置、字段与首错顺序；仅由
// world_codec.cpp 登记包含。
Outcome<InitialWorldSeed> decode_world_seed(const BoundaryValue &source,
                                            const PreparedCatalog &catalog) {
  InitialWorldSeed result;
  auto decoded =
      decode_world_seed_into(source, catalog, result, nullptr, nullptr);
  if (!decoded.ok)
    return {false, {}, std::move(decoded.error)};
  return {true, std::move(result), {}};
}
// One decoder body writes directly into the final seed owner. The reservation
// covers only connected ordinary producers; raw manual branches are not
// credited.
struct WorldJsonDecoder {
  static Outcome<JsonDecodedWorldSeedOwner>
  decode(const BoundaryValue &source,
         std::shared_ptr<const PreparedCatalog> catalog,
         const CapacityProfile &capacity, CodecDecodeBudget &budget,
         CheckpointJobBudget &job) {
    auto profile = job.check_profile(capacity);
    if (!profile.ok)
      throw checkpoint_job_detail::FailureException{std::move(profile.error)};
    auto begun = JsonDecodedWorldSeedOwner::begin(catalog, budget, job);
    if (!begun.ok)
      throw checkpoint_job_detail::FailureException{std::move(begun.error)};
    try {
      ScopedCheckpointMemory memory(begun.value.memory_resource());
      ScopedCheckpointBoundaryDecode boundary(budget, job,
                                              begun.value.memory_resource());
      auto decoded = decode_world_seed_into(
          source, *catalog, begun.value.candidate(), &budget, &job);
      if (!decoded.ok)
        return {false, {}, std::move(decoded.error)};
      auto sealed = begun.value.seal(budget, job);
      if (!sealed.ok)
        throw checkpoint_job_detail::FailureException{std::move(sealed.error)};
      return {true, std::move(begun.value), {}};
    } catch (const std::bad_alloc &) {
      throw checkpoint_job_detail::FailureException{job.capture_fixed_failure(
          CheckpointJobFailureCode::allocation_failed)};
    } catch (const std::length_error &) {
      throw checkpoint_job_detail::FailureException{
          job.capture_fixed_failure(CheckpointJobFailureCode::size_overflow)};
    }
  }
};
Outcome<JsonDecodedWorldSeedOwner>
decode_world_seed_owned(const BoundaryValue &source,
                        std::shared_ptr<const PreparedCatalog> catalog,
                        const CapacityProfile &capacity,
                        CodecDecodeBudget &budget, CheckpointJobBudget &job) {
  return WorldJsonDecoder::decode(source, std::move(catalog), capacity, budget,
                                  job);
}
