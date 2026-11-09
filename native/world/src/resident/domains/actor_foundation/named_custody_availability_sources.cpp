#include "resident/domains/military/military_model.hpp"
// term 的 actor ordinal/Field 应用 M 与 closure 的关闭来源全由原军务 reader
// 核定。
#include "named_custody_availability_sources.hpp"
namespace hanmo::resident {
bool NativeNamedCustodyAvailabilityRead::bound_to(
    const CommonStore &common, const MilitaryState &military,
    const AuthoredActorFoundationState &foundation,
    const ActorFoundationCatalog &catalog) const noexcept {
  return &x_.custody.common == &common && &x_.custody.military == &military &&
         &x_.foundation == &foundation && &x_.actors == &catalog;
}
Outcome<const NamedMilitaryCustodyTerm *>
NativeNamedCustodyAvailabilityRead::read_capture(
    const SourceRef &source) const {
  return read_original_named_military_custody_term(x_.custody, source);
}
Outcome<const NamedMilitaryCustodyRelease *>
NativeNamedCustodyAvailabilityRead::read_release(
    const SourceRef &source) const {
  return read_original_named_military_custody_release(x_, source);
}
Status NativeNamedCustodyAvailabilityRead::visit_original_sources(
    NamedCustodyAvailabilitySourceSink &sink) const {
  auto valid = validate_named_military_custody_release_sources(x_);
  if (!valid.ok)
    return valid;
  for (const auto &term : x_.custody.military.named_custody.terms) {
    valid = sink.capture(term);
    if (!valid.ok)
      return valid;
  }
  for (const auto &closure : x_.custody.military.named_custody.closures) {
    valid = sink.release(closure);
    if (!valid.ok)
      return valid;
  }
  return common_success();
}
} // namespace hanmo::resident
