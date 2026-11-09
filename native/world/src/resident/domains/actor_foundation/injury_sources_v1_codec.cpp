#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "injury_sources_v1_codec.hpp"
#include "actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/storage/common_checkpoint.hpp"
namespace hanmo::resident {
namespace {
bool id(const StableId &value) { return !value.empty() && value.size() <= 256; }
template <class C> Status progress(C &c, GrowthRecoveryProgress &value) {
  c.field("injury", value.injury);
  c.field("points", value.points);
  if (c.status().ok &&
      (value.injury < 0 || value.injury > 3 || value.points < 0 ||
       value.points > 3 || (!value.injury && value.points)))
    return c.reject("InvalidCharacterInjuryProgress");
  return c.status();
}
template <class C>
Status original(C &c, CharacterInjuryOriginalSourceV1 &value) {
  Integer tag = static_cast<Integer>(value.index());
  c.field("tag", tag);
  if (!c.status().ok)
    return c.status();
  if (tag < 0 || tag > 2)
    return c.reject("UnsupportedCharacterInjurySourceVersion1");
  if (c.reading()) {
    if (tag == 0)
      value = CharacterBattleReceiptSourceV1{};
    else if (tag == 1)
      value = OriginalRestProvidedCause{};
    else
      value = CharacterTreatmentActionSourceV1{};
  }
  if (tag == 0) {
    auto &source = std::get<CharacterBattleReceiptSourceV1>(value);
    c.object("original_receipt", [&](auto &v) {
      v.field("domain", source.original_receipt.domain);
      v.field("kind", source.original_receipt.kind);
      return v.field("id", source.original_receipt.id);
    });
    c.field("source_schema", source.source_schema);
    const auto &ref = source.original_receipt;
    if (c.status().ok &&
        (source.source_schema != 1 || !id(ref.id) ||
         !((ref.domain == "military" && ref.kind == "military_casualties") ||
           (ref.domain == "personal" && ref.kind == "personal_battle"))))
      return c.reject("CharacterInjuryOriginalBattleReceiptRequired");
    return c.status();
  }
  if (tag == 1) {
    auto &source = std::get<OriginalRestProvidedCause>(value);
    c.field("action_id", source.action_id);
    c.field("rest_kind", source.rest_kind);
    c.field("provider_actor_id", source.provider_actor_id);
    c.field("site_id", source.site_id);
    c.field("provided_half_days", source.provided_half_days);
    c.field("service_fulfillment_ref", source.service_fulfillment_ref);
    if (c.status().ok &&
        (!id(source.action_id) || source.rest_kind != "rest" ||
         !id(source.provider_actor_id) || !id(source.site_id) ||
         source.provided_half_days < 1 || !id(source.service_fulfillment_ref)))
      return c.reject("CharacterInjuryOriginalRestProvidedRequired");
    return c.status();
  }
  auto &source = std::get<CharacterTreatmentActionSourceV1>(value);
  c.field("action_id", source.action_id);
  c.field("completion_ordinal", source.completion_ordinal);
  if (c.status().ok &&
      (!id(source.action_id) || source.completion_ordinal != 1))
    return c.reject("CharacterInjuryTerminalTreatmentOrdinalRequired");
  return c.status();
}
template <class C> Status row(C &c, CharacterInjuryApplicationV1 &value) {
  c.field("actor_id", value.actor_id);
  c.object("original_source",
           [&](auto &v) { return original(v, value.original_source); });
  c.object("before",
           [&](auto &v) { return progress(v, value.progress.before); });
  c.object("after", [&](auto &v) { return progress(v, value.progress.after); });
  c.field("offered_points", value.progress.offered_points);
  c.field("used_points", value.progress.used_points);
  if (c.status().ok &&
      (value.progress.offered_points < 0 || value.progress.used_points < 0 ||
       value.progress.used_points > value.progress.offered_points ||
       value.progress.used_points >
           4 * value.progress.before.injury - value.progress.before.points))
    return c.reject("InvalidCharacterInjuryCredit");
  if (c.status().ok &&
      std::holds_alternative<CharacterBattleReceiptSourceV1>(
          value.original_source) &&
      (value.progress.offered_points || value.progress.used_points))
    return c.reject("BattleSourceCannotGrantRecoveryPoints");
  if (c.status().ok && std::holds_alternative<CharacterTreatmentActionSourceV1>(
                           value.original_source)) {
    const auto &credit = value.progress;
    const Integer need = 4 * credit.before.injury - credit.before.points;
    const Integer used =
        credit.offered_points < need ? credit.offered_points : need;
    const Integer total = credit.before.points + used;
    const Integer injury = credit.before.injury - total / 4;
    if (credit.offered_points < 1 || credit.used_points != used ||
        credit.after.injury != injury ||
        credit.after.points != (injury ? total % 4 : 0))
      return c.reject("InvalidTerminalTreatmentCreditVersion1");
  }
  if (c.status().ok && !id(value.actor_id))
    return c.reject("CharacterInjuryActorRequired");
  // Exact source formula, once and actual M are proved by the paired source
  // reader/replay, not by interpreting today's actor or a codec-supplied
  // amount.
  return c.status();
}
template <class C>
Status keys(C &c, const CharacterInjurySourcesStateV1 &value) {
  if (!c.status().ok)
    return c.status();
  for (const auto &item : value.applications.rows())
    if (!id(item.first))
      return c.reject("CharacterInjuryApplicationKeyRequired", item.first);
  return c.status();
}
template <class C> Status fields(C &c, CharacterInjurySourcesStateV1 &value) {
  c.table("applications", value.applications,
          [](auto &v, auto &item) { return row(v, item); });
  return keys(c, value);
}
Status fields(CodecVisitor &c, CharacterInjurySourcesStateV1 &value) {
  common_checkpoint::table(c, "applications", value.applications,
                           [](auto &v, auto &item) { return row(v, item); });
  return keys(c, value);
}
} // namespace
Status visit_character_injury_sources_v1(CodecVisitor &c,
                                         CharacterInjurySourcesStateV1 &value) {
  return fields(c, value);
}
Status emit_character_injury_sources_v1_projection(
    ProjectionWriter &sink, std::string_view name,
    const CharacterInjurySourcesStateV1 &value) {
  return emit_projection_record<ActorFoundationProjectionArchive>(
      sink, name, value, [](auto &c, auto &item) { return fields(c, item); });
}
Outcome<BoundaryValue>
encode_character_injury_sources_v1(const CharacterInjurySourcesStateV1 &value) {
  BoundaryValue result;
  BoundaryProjectionWriter sink(result);
  auto status = emit_character_injury_sources_v1_projection(sink, "", value);
  if (status.ok)
    status = sink.finish();
  return {status.ok, std::move(result), status.error};
}
Outcome<CharacterInjurySourcesStateV1>
decode_character_injury_sources_v1(const BoundaryValue &input) {
  CharacterInjurySourcesStateV1 result;
  ActorFoundationJsonVisitor c(input);
  auto status = fields(c, result);
  if (status.ok)
    status = c.finish();
  return {status.ok, std::move(result), status.error};
}
} // namespace hanmo::resident
