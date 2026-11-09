// 原八根三出口同表；写侧纯借读，冷值必须交实际World完整验证后才可发布。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "treaty_terms_state_codec_fields.hpp"
namespace hanmo::resident {
Status visit_political_treaty_terms(CodecVisitor &c,
                                    PoliticalTreatyTermsState &r) {
  return treaty_terms_state_codec_fields::fields(c, r);
}
Status
emit_political_treaty_terms_projection(ProjectionWriter &out,
                                       std::string_view name,
                                       const PoliticalTreatyTermsState &r) {
  return emit_projection_record<ActorFoundationProjectionArchive>(
      out, name, r, [](auto &c, auto &v) {
        return treaty_terms_state_codec_fields::fields(c, v);
      });
}
Outcome<BoundaryValue>
encode_political_treaty_terms(const PoliticalTreatyTermsState &r) {
  BoundaryValue out;
  BoundaryProjectionWriter writer(out);
  auto valid = emit_political_treaty_terms_projection(writer, "", r);
  if (valid.ok)
    valid = writer.finish();
  return {valid.ok, std::move(out), valid.error};
}
Outcome<PoliticalTreatyTermsState>
decode_political_treaty_terms(const BoundaryValue &input) {
  PoliticalTreatyTermsState out;
  ActorFoundationJsonVisitor reader(input);
  auto valid = treaty_terms_state_codec_fields::fields(reader, out);
  if (valid.ok)
    valid = reader.finish();
  return {valid.ok, std::move(out), valid.error};
}
} // namespace hanmo::resident
