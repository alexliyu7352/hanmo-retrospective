#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "political_continuity_codec.hpp"
#include "political_continuity_codec_fields.hpp"
namespace hanmo::resident {
Status visit_political_continuity(CodecVisitor &codec,
                                  PoliticalContinuityState &value) {
  return political_continuity_fields::fields(codec, value);
}
Status
emit_political_continuity_projection(ProjectionWriter &writer,
                                     std::string_view name,
                                     const PoliticalContinuityState &value) {
  auto valid = validate_political_continuity_shape(value);
  if (!valid.ok)
    return valid;
  return emit_projection_record<ActorFoundationProjectionArchive>(
      writer, name, value, [](auto &fields, auto &row) {
        return political_continuity_fields::fields(fields, row);
      });
}
Outcome<BoundaryValue>
encode_political_continuity(const PoliticalContinuityState &value) {
  BoundaryValue output;
  BoundaryProjectionWriter writer(output);
  auto valid = emit_political_continuity_projection(writer, "", value);
  if (valid.ok)
    valid = writer.finish();
  return {valid.ok, std::move(output), valid.error};
}
Outcome<PoliticalContinuityState>
decode_political_continuity(const BoundaryValue &input) {
  ActorFoundationJsonVisitor reader(input);
  auto valid = reader.prepare_default_values<PoliticalContinuityState>();
  if (!valid.ok)
    return {false, {}, valid.error};
  PoliticalContinuityState value;
  valid = political_continuity_fields::fields(reader, value);
  if (valid.ok)
    valid = reader.finish();
  if (valid.ok)
    valid = validate_political_continuity_shape(value);
  // 这里只返回未安装值；World全来源不齐时不能据此打开profile或保存成功档。
  return {valid.ok, std::move(value), valid.error};
}
} // namespace hanmo::resident
