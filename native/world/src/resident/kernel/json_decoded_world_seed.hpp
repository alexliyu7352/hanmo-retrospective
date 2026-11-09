// 显式JSON冷解码的封闭拥有者；不让公开InitialWorldSeed丢失其费用租约。
#pragma once
#include "resident/checkpoint/checkpoint_capacity.hpp"
#include "resident/checkpoint/checkpoint_job_memory.hpp"
namespace hanmo::resident {
struct InitialWorldSeed;
struct PreparedCatalog;
struct CommonState;
struct WorldBundle;
struct BoundaryValue;
struct WorldJsonDecoder;
class JsonDecodedSeedState;
class ResidentWorld;
class JsonDecodedWorldSeedOwner {
public:
  JsonDecodedWorldSeedOwner() noexcept = default;
  JsonDecodedWorldSeedOwner(JsonDecodedWorldSeedOwner &&) noexcept = default;
  JsonDecodedWorldSeedOwner &
  operator=(JsonDecodedWorldSeedOwner &&) noexcept = default;
  JsonDecodedWorldSeedOwner(const JsonDecodedWorldSeedOwner &) = delete;
  JsonDecodedWorldSeedOwner &
  operator=(const JsonDecodedWorldSeedOwner &) = delete;

private:
  friend struct WorldJsonDecoder;
  friend struct WorldBundle;
  friend class ResidentWorld;
  std::shared_ptr<JsonDecodedSeedState> state_;
  static CheckpointJobResult<JsonDecodedWorldSeedOwner>
  begin(std::shared_ptr<const PreparedCatalog>, CodecDecodeBudget &,
        CheckpointJobBudget &);
  InitialWorldSeed &candidate();
  std::shared_ptr<CheckpointMemoryResource> memory_resource() const noexcept;
  CheckpointJobStatus seal(CodecDecodeBudget &, CheckpointJobBudget &);
  std::shared_ptr<const PreparedCatalog>
      require_catalog(std::shared_ptr<const PreparedCatalog>) const;
  InitialWorldSeed &install_seed();
  InitialWorldSeed &&consume_seed();
  std::shared_ptr<const JsonDecodedSeedState> release_owner() noexcept;
};
// The original decoder fills this owner directly under one job/Boundary scope.
// Its token covers actual connected ordinary allocations, not every manual
// decoder branch. Ownership is complete; whole-decoder first-charge is not.
// Storage retention still enumerates actual owners and rejects untracked roots.
// Business failures retain the original Status error. Technical failures carry
// their owning FailureException through the actual CLI/Godot catch boundary.
Outcome<JsonDecodedWorldSeedOwner> decode_world_seed_owned(
    const BoundaryValue &, std::shared_ptr<const PreparedCatalog>,
    const CapacityProfile &, CodecDecodeBudget &, CheckpointJobBudget &);
// Keep the original immutable catalog and its connected ordinary token
// together. This exact shared deleter owns the source, not a paid flag for
// unknown roots.
std::shared_ptr<const PreparedCatalog>
seal_json_decoded_catalog(std::shared_ptr<const PreparedCatalog>,
                          CodecDecodeBudget &, CheckpointJobBudget &);
bool has_json_decoded_catalog_owner(
    const std::shared_ptr<const PreparedCatalog> &) noexcept;
Status retain_json_decoded_catalog_owner(
    const std::shared_ptr<const PreparedCatalog> &,
    CheckpointRetentionCensus &);
// Enumerate the complete actual preimage and adopted Common; partial ordinary
// reservations remain alive but do not grant free credit to unrelated fields.
Status retain_json_decoded_seed_owner(
    const std::shared_ptr<const JsonDecodedSeedState> &, const CommonState &,
    CheckpointRetentionCensus &);
} // namespace hanmo::resident
