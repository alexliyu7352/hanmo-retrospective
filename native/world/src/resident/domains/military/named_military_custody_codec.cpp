// JSON 与双 binary 共用同一有限字段表；未接 owning graph 时明确拒绝，不回报空预算成功。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "named_military_custody_codec.hpp"
#include "named_military_custody_codec_fields.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
namespace hanmo::resident {
Status visit_named_military_custody_history(CodecVisitor &codec,
    NamedMilitaryCustodyHistory &state, const MovementRecordCodecSelection &selection) {

  if (codec.reading() && (!state.terms.empty() || !state.closures.empty() || !state.field_order.empty()))
    return codec.reject("NamedMilitaryCustodyDecodeRequiresEmpty");
  return named_custody_fields::value(codec, state, selection);
}
Outcome<NamedMilitaryCustodyHistory> decode_named_military_custody_history(
    const BoundaryValue &input, const MovementRecordCodecSelection &selection) {
  ActorFoundationJsonVisitor reader(input);
  auto ready = reader.prepare_default_values<NamedMilitaryCustodyHistory>();
  if (!ready.ok) return {false, {}, ready.error};
  NamedMilitaryCustodyHistory state;
  auto decoded = named_custody_fields::value(reader, state, selection);
  if (!decoded.ok) return {false, {}, decoded.error};
  auto finished = reader.finish();
  if (!finished.ok) return {false, {}, finished.error};
  return {true, std::move(state), {}};
}
Status emit_named_military_custody_history(ProjectionWriter &sink, std::string_view name,
    const NamedMilitaryCustodyHistory &state, const MovementRecordCodecSelection &selection) {
  return emit_projection_record<ActorFoundationProjectionArchive>(sink, name, state,
      [&](auto &archive, auto &value) { return named_custody_fields::value(archive, value, selection); });
}
Status emit_named_military_custody_fact_payloads_fields(ProjectionWriter &sink,
    const NamedMilitaryCustodyHistory &state) {
  const auto emit = [&](const StableId &id, const char *kind) {
    return sink.object(id, [&](auto &record) {
      record.text("kind", kind);
      return record.text("domain", "military");
    });
  };
  for (const auto &term : state.terms) {
    auto result = emit(term.id, "military_named_custody_term");
    if (!result.ok) return result;
  }
  for (const auto &release : state.closures) {
    auto result = emit(release.id, "military_named_custody_released");
    if (!result.ok) return result;
  }
  return sink.status();
}
Outcome<NamedMilitaryCustodyPolicy> decode_named_military_custody_policy(const BoundaryValue &input) {
  ActorFoundationJsonVisitor reader(input);
  auto ready = reader.prepare_default_values<NamedMilitaryCustodyPolicy>();
  if (!ready.ok) return {false, {}, ready.error};
  NamedMilitaryCustodyPolicy policy;
  // policy 只有规则 ID 和作者半日数，不消费 movement 选择或默认时长。
  reader.field("rule_id", policy.rule_id);
  reader.field("duration_half_days", policy.duration_half_days);
  auto finished = reader.finish();
  if (!finished.ok) return {false, {}, finished.error};
  if (policy.rule_id.empty() || policy.duration_half_days <= 0)
    return {false, {}, common_failure("NamedMilitaryCustodyPolicyRequired").error};
  return {true, std::move(policy), {}};
}
} // namespace hanmo::resident
