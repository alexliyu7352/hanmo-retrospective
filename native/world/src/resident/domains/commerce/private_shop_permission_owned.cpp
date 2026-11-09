// 只复制原许可的五个文本字段；规则仍由原司法 full reader 唯一执行。
#include "private_shop_contract.hpp"
namespace hanmo::resident {
namespace {

PrivateShopPermissionSource copy_fields(const WorldIdentity &identity,
                                        const PrivateShopPermissionRequest &r,
                                        const StableId &permit,
                                        const StableId &clearance,
                                        Integer issued, Integer observed,
                                        Integer through) {
  return {identity,
          {checkpoint_fresh_string_copy(r.actor_id),
           checkpoint_fresh_string_copy(r.premises_id),
           checkpoint_fresh_string_copy(r.place_id)},
          checkpoint_fresh_string_copy(permit),
          checkpoint_fresh_string_copy(clearance),
          issued,
          observed,
          through};
}
} // namespace

PrivateShopPermissionSource copy_private_shop_permission_source_preleased(
    const PrivateShopPermissionSource &s) {
  return copy_fields(s.source_identity, s.request, s.permit_fact_id,
                     s.clearance_fact_id, s.issued_tick, s.observed_tick,
                     s.valid_through_tick);
}
OwnedPrivateShopPermissionSource own_private_shop_permission_source(
    const WorldIdentity &identity, const PrivateShopPermissionRequest &request,
    const StableId &permit, const StableId &clearance, Integer issued,
    Integer observed, Integer through) {
  OwnedPrivateShopPermissionSource out;


  (out.memory_).grow(0);
  out.source_ = copy_fields(identity, request, permit, clearance, issued,
                            observed, through);
  return out;
}
namespace {
OwnedPrivateShopPermissionSource
copy_owned_permission(const PrivateShopPermissionSource &s) {
  return own_private_shop_permission_source(
      s.source_identity, s.request, s.permit_fact_id, s.clearance_fact_id,
      s.issued_tick, s.observed_tick, s.valid_through_tick);
}
} // namespace
PrivateShopPermissionValue::PrivateShopPermissionValue(
    const PrivateShopPermissionValue &other)
    : source_(std::in_place, copy_owned_permission(other.source())) {}
PrivateShopPermissionValue &
PrivateShopPermissionValue::operator=(const PrivateShopPermissionValue &other) {
  if (this == &other)
    return *this;
  auto fresh = copy_owned_permission(other.source());
  source_.reset();
  source_.emplace(std::move(fresh));
  return *this;
}
PrivateShopPermissionValue &PrivateShopPermissionValue::operator=(
    PrivateShopPermissionValue &&other) noexcept {
  if (this == &other)
    return *this;
  source_.reset();
  source_.emplace(std::move(*other.source_));
  return *this;
}
} // namespace hanmo::resident
