// navigation_observation 原强类型载荷独立拥有、复制、来源出口和三种原 codec 回调。
#include "field_event_navigation_observation.hpp"
#include "field_event_typed_codec.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include <stdexcept>
namespace hanmo::resident {
namespace {
class Payload final : public FieldEventPayloadValue {
public:
  std::optional<FieldNavigationObservation> value;
  Integer type_id() const noexcept override { return field_event_navigation_observation_payload_id; }
  std::shared_ptr<FieldEventPayloadValue> clone() const override { return checkpoint_make_shared<Payload>(*this); }
  void visit_sources(FieldEventSourceSink &sink) const override { if (value && value->original_command_result) sink.source(*value->original_command_result); }
};
Payload &typed(FieldEventPayloads &values) {
  auto *result = dynamic_cast<Payload *>(&values.ensure(2));
  if (!result) throw std::logic_error("FieldEventPayloadTypeMismatch");
  return *result;
}
const Payload *typed(const FieldEventPayloads &values) {
  const auto *base = values.find(2);
  if (!base) return nullptr;
  const auto *result = dynamic_cast<const Payload *>(base);
  if (!result) throw std::logic_error("FieldEventPayloadTypeMismatch");
  return result;
}
template <class C> Status fields(C &c, FieldNavigationObservation &v) {
  using field_event_fields::field;
  field(c, "evaluated_at", v.evaluated_at);
  field(c, "mission", v.mission);
  field(c, "mission_revision", v.mission_revision);
  field(c, "mission_transition_sequence", v.mission_transition_sequence);
  field(c, "original_command_result", v.original_command_result);
  field(c, "original_ai_source_id", v.original_ai_source_id);
  field(c, "original_order_id", v.original_order_id);
  field(c, "routing", v.routing);
  field(c, "ai_pursuit", v.ai_pursuit);
  field(c, "from_dm", v.from_dm);
  field(c, "target_dm", v.target_dm);
  field(c, "target_formation_id", v.target_formation_id);
  field(c, "target_motion_sequence", v.target_motion_sequence);
  return c.status();
}
template<class C> Status codec(C &c, FieldEventPayloads &values) {
  // 写端空槽只借局部空 optional；不为输出偷偷修改已发布状态。
  std::optional<FieldNavigationObservation> empty;
  auto *existing = typed(static_cast<const FieldEventPayloads &>(values));
  auto &value = c.reading() ? typed(values).value
                          : (existing ? const_cast<std::optional<FieldNavigationObservation> &>(existing->value) : empty);
  return c.object("navigation_observation", [&](auto &child) {
    return child.optional("optional", value, [](auto &record, auto &row) { return fields(record,row); });
  });
}
} // namespace
std::optional<FieldNavigationObservation> &field_event_navigation_observation(FieldBattleEvent &event) { return typed(event.payloads).value; }
const std::optional<FieldNavigationObservation> &field_event_navigation_observation(const FieldBattleEvent &event) {
  static const std::optional<FieldNavigationObservation> empty;
  const auto *p = typed(event.payloads); return p ? p->value : empty;
}
const FieldEventPayloadRegistration &field_event_navigation_observation_registration() {
  static const FieldEventPayloadRegistration value{
    2, "navigation_observation", -1,
    []() -> std::shared_ptr<FieldEventPayloadValue> { return checkpoint_make_shared<Payload>(); },
    &codec<CodecVisitor>, &codec<ActorFoundationJsonVisitor>, &codec<ActorFoundationProjectionArchive>};
  return value;
}
} // namespace hanmo::resident
