// mission_transition 原强类型载荷独立拥有、复制、来源出口和三种原 codec 回调。
#include "field_event_mission_transition.hpp"
#include "field_event_typed_codec.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include <stdexcept>
namespace hanmo::resident {
namespace {
class Payload final : public FieldEventPayloadValue {
public:
  std::optional<FieldMissionTransition> value;
  Integer type_id() const noexcept override { return field_event_mission_transition_payload_id; }
  std::shared_ptr<FieldEventPayloadValue> clone() const override { return checkpoint_make_shared<Payload>(*this); }
  void visit_sources(FieldEventSourceSink &sink) const override { if (value && value->original_command_result) sink.source(*value->original_command_result); }
};
Payload &typed(FieldEventPayloads &values) {
  auto *result = dynamic_cast<Payload *>(&values.ensure(1));
  if (!result) throw std::logic_error("FieldEventPayloadTypeMismatch");
  return *result;
}
const Payload *typed(const FieldEventPayloads &values) {
  const auto *base = values.find(1);
  if (!base) return nullptr;
  const auto *result = dynamic_cast<const Payload *>(base);
  if (!result) throw std::logic_error("FieldEventPayloadTypeMismatch");
  return result;
}
template<class C> Status reason(C &c, FieldMissionTransitionReason &v) {
  Integer tag=static_cast<Integer>(v);c.field("tag",tag);
  if(c.status().ok&&(tag<0||tag>5)) return c.reject("InvalidFieldBattleTag");
  if(c.status().ok&&c.reading())v=static_cast<FieldMissionTransitionReason>(tag);
  return c.status();
}
template <class C> Status fields(C &c, FieldMissionTransition &v) {
  using field_event_fields::field;
  c.object("reason", [&](auto &child) { return reason(child, v.reason); });
  field(c, "from", v.from);
  field(c, "to", v.to);
  field(c, "from_ai_pursuit", v.from_ai_pursuit);
  field(c, "to_ai_pursuit", v.to_ai_pursuit);
  field(c, "mission_revision", v.mission_revision);
  field(c, "previous_sequence", v.previous_sequence);
  field(c, "original_command_result", v.original_command_result);
  field(c, "original_ai_source_id", v.original_ai_source_id);
  field(c, "original_order_id", v.original_order_id);
  field(c, "at_position_dm", v.at_position_dm);
  field(c, "target_formation_id", v.target_formation_id);
  field(c, "trigger_event_sequence", v.trigger_event_sequence);
  field(c, "original_flank_assignment_index",
        v.original_flank_assignment_index);
  return c.status();
}
template<class C> Status codec(C &c, FieldEventPayloads &values) {
  // 写端空槽只借局部空 optional；不为输出偷偷修改已发布状态。
  std::optional<FieldMissionTransition> empty;
  auto *existing = typed(static_cast<const FieldEventPayloads &>(values));
  auto &value = c.reading() ? typed(values).value
                          : (existing ? const_cast<std::optional<FieldMissionTransition> &>(existing->value) : empty);
  return c.object("mission_transition", [&](auto &child) {
    return child.optional("optional", value, [](auto &record, auto &row) { return fields(record,row); });
  });
}
} // namespace
std::optional<FieldMissionTransition> &field_event_mission_transition(FieldBattleEvent &event) { return typed(event.payloads).value; }
const std::optional<FieldMissionTransition> &field_event_mission_transition(const FieldBattleEvent &event) {
  static const std::optional<FieldMissionTransition> empty;
  const auto *p = typed(event.payloads); return p ? p->value : empty;
}
const FieldEventPayloadRegistration &field_event_mission_transition_registration() {
  static const FieldEventPayloadRegistration value{
    1, "mission_transition", -1,
    []() -> std::shared_ptr<FieldEventPayloadValue> { return checkpoint_make_shared<Payload>(); },
    &codec<CodecVisitor>, &codec<ActorFoundationJsonVisitor>, &codec<ActorFoundationProjectionArchive>};
  return value;
}
} // namespace hanmo::resident
