// 审核草案：仅借同一真实 World；clock/owning scope/索引任何缺口都原样拒绝。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "organization_grant_route.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/domains/organization/organization_authority_codec_fields.hpp"
#include "resident/domains/politics/political_exact_phase.hpp"
#include "resident/kernel/exact_time_profile.hpp"
#include "resident/kernel/world_bundle.hpp"
#include "resident/kernel/world_read_stamp.hpp"
namespace hanmo::resident {
namespace {
class WorldOrganizationAuthorityClock final
    : public OrganizationAuthorityClock {
public:
  explicit WorldOrganizationAuthorityClock(const WorldBundle &world)
      : world_(world) {}
  Outcome<WorldMoment> read_organization_authority_now() const override {
    return world_.actual_phase_moment();
  }

private:
  const WorldBundle &world_;
};
bool complete(const WorldBundle &world) {
  return world.catalog->exact_time &&
         exact_complete_selected(*world.catalog->exact_time);
}
Status check_command_boundary(const WorldBundle &world, const StableId &actor,
                              const StableId &kind,
                              const GrantChangeCommand &command) {
  if (!world.dynamic_enabled() || !world.catalog->organization)
    return common_failure("DynamicProfileRequired");
  if (actor != command.acting.actor_id)
    return common_failure("CommandActorMismatch");
  const bool matches =
      (kind == "issue_grant" && command.kind == GrantChangeKind::issue) ||
      (kind == "amend_grant" && command.kind == GrantChangeKind::amend) ||
      (kind == "revoke_grant" && command.kind == GrantChangeKind::revoke);
  if (!matches)
    return common_failure("CommandPayloadKindMismatch");
  if (!world.catalog->organization->authority_lifecycle_enabled)
    return common_failure("RuntimeGrantCapabilityRequired");
  return common_success();
}
Outcome<std::unique_ptr<PhaseCurrentReadScope>> enter(const WorldBundle &world,
                                                      const WorldMoment &at) {
  if (!complete(world) || world.has_active_read_phase())
    return {true, {}, {}};
  return world.enter_phase_read_context("organization_grant_command",
                                        "organization", {at, at}, at);
}
// 无论唯一 writer 成功、普通失败还是抛出，都让下一 consumer 重取本候选票。
struct WriterBoundary {
  WorldBundle &world;
  ~WriterBoundary() { world.note_write_boundary(); }
};
} // namespace
bool organization_grant_command_kind(const StableId &kind) {
  return kind == "issue_grant" || kind == "amend_grant" ||
         kind == "revoke_grant";
}
Outcome<GrantChangeCommand>
decode_organization_grant_command(const StableId &kind,
                                  const BoundaryValue &params) {
  if (!organization_grant_command_kind(kind))
    return {false, {}, common_failure("UnsupportedAction", kind).error};
  GrantChangeCommand command;
  command.kind = kind == "issue_grant"   ? GrantChangeKind::issue
                 : kind == "amend_grant" ? GrantChangeKind::amend
                                         : GrantChangeKind::revoke;
  ActorFoundationJsonVisitor reader(params);
  // Grant/ActingAuthority/Scope 的既有原字段 walker 负责严格形状；不持久请求
  // JSON。
  organization_authority_codec_fields::value(reader, "acting", command.acting,
                                             false);
  reader.field("policy_id", command.policy_id);
  reader.field("grant_id", command.grant_id);
  organization_authority_codec_fields::value(reader, "terms", command.terms,
                                             false);
  organization_authority_codec_fields::value(reader, "parent_grant_id",
                                             command.parent_grant_id, false);
  reader.field("expected_grant_revision", command.expected_grant_revision);
  reader.field("expected_parent_revision", command.expected_parent_revision);
  reader.field("expected_office_epoch", command.expected_office_epoch);
  auto valid = reader.finish();
  return {valid.ok, std::move(command), valid.error};
}

Status preview_organization_grant_command(const WorldBundle &world,
                                          const StableId &actor,
                                          const StableId &kind,
                                          const GrantChangeCommand &command) {
  auto checked = check_command_boundary(world, actor, kind, command);
  if (!checked.ok)
    return checked;
  WorldOrganizationAuthorityClock clock(world);
  auto at = clock.read_organization_authority_now();
  if (!at.ok)
    return {false, {}, at.error};
  auto phase = enter(world, at.value);
  if (!phase.ok)
    return {false, {}, phase.error};
  // 按原完整模式选中 owner，不以 runtime 存在或空 reader 冒充完整装配。
  std::shared_ptr<const PoliticalExactReadLease> lease;
  if (complete(world)) {
    auto read = bind_original_political_exact_read(
        world, at.value, native_political_exact_world_factory());
    if (!read.ok)
      return {false, {}, read.error};
    lease = std::move(read.value);
    if (!world.organization_runtime ||
        &lease->bindings().organization.runtime != &*world.organization_runtime)
      return common_failure("PoliticalExactReadBindingMismatch");
  }
  return preview_grant_change(world.common, world.organization,
                              *world.catalog->organization, command, at.value,
                              clock);
}
Outcome<OrganizationEffects>
apply_organization_grant_command(WorldBundle &world, const StableId &actor,
                                 const StableId &kind,
                                 const GrantChangeCommand &command) {
  auto checked = check_command_boundary(world, actor, kind, command);
  if (!checked.ok)
    return {false, {}, checked.error};
  // 必须在未发布的新候选上重建全部借用；不带预览世界的 quote/permit/clock。
  WorldOrganizationAuthorityClock clock(world);
  auto at = clock.read_organization_authority_now();
  if (!at.ok)
    return {false, {}, at.error};
  auto phase = enter(world, at.value);
  if (!phase.ok)
    return {false, {}, phase.error};
  std::shared_ptr<PoliticalExactWriteLease> lease;
  if (complete(world)) {
    auto write = bind_original_political_exact_write(
        world, at.value, native_political_exact_world_factory());
    if (!write.ok)
      return {false, {}, write.error};
    lease = std::move(write.value);
    if (!world.organization_runtime ||
        &lease->bindings().organization.original_runtime.runtime !=
            &*world.organization_runtime)
      return {false,
              {},
              common_failure("PoliticalExactWriteBindingMismatch").error};
  }
  WriterBoundary invalidation{world};
  auto fact = apply_grant_change_in_candidate(world.common, world.organization,
                                              *world.catalog->organization,
                                              command, at.value, clock);
  if (!fact.ok)
    return {false, {}, fact.error};
  if (world.organization_runtime) {
    auto indexed = index_original_grant_versions_in_candidate(
        world.common, world.organization, *world.organization_runtime);
    if (!indexed.ok)
      return {false, {}, indexed.error};
  }
  // 不另造领域回执；沿现有 OrganizationEffects 的原事实引用返回。
  OrganizationEffects effect;
  effect.fact_ids.push_back(std::move(fact.value));
  return {true, std::move(effect), {}};
}
} // namespace hanmo::resident
