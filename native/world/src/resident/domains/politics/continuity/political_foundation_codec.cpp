#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "political_foundation_codec_fields.hpp"

namespace hanmo::resident {
Status
visit_foundation_lifecycle_decision(CodecVisitor &c,
                                    PoliticalLifecycleDecisionRecord &value) {
  return foundation_codec_fields::decision(c, value);
}
Status emit_foundation_lifecycle_decision_projection(
    ProjectionWriter &out, std::string_view name,
    const PoliticalLifecycleDecisionRecord &value) {
  return emit_projection_record<ActorFoundationProjectionArchive>(
      out, name, value, [](auto &c, auto &row) {
        return foundation_codec_fields::decision(c, row);
      });
}
Outcome<BoundaryValue> encode_foundation_lifecycle_decision(
    const PoliticalLifecycleDecisionRecord &value) {
  BoundaryValue result;
  BoundaryProjectionWriter writer(result);
  auto valid = emit_foundation_lifecycle_decision_projection(writer, "", value);
  if (valid.ok)
    valid = writer.finish();
  return {valid.ok, std::move(result), valid.error};
}
Outcome<PoliticalLifecycleDecisionRecord>
decode_foundation_lifecycle_decision(const BoundaryValue &input) {
  ActorFoundationJsonVisitor reader(input);
  auto valid =
      reader.prepare_default_values<PoliticalLifecycleDecisionRecord>();
  if (!valid.ok)
    return {false, {}, valid.error};
  PoliticalLifecycleDecisionRecord result;
  valid = foundation_codec_fields::decision(reader, result);
  if (valid.ok)
    valid = reader.finish();
  return {valid.ok, std::move(result), valid.error};
}
} // namespace hanmo::resident
