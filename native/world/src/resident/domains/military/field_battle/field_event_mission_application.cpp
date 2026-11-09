// mission_application 原强类型载荷独立拥有、复制、来源出口和三种原 codec 回调。
#include "field_event_mission_application.hpp"
#include "field_event_typed_codec.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include <stdexcept>
namespace hanmo::resident {
namespace {
class Payload final : public FieldEventPayloadValue {
public:
  std::optional<FieldMissionApplication> value;
  Integer type_id() const noexcept override { return field_event_mission_application_payload_id; }
  std::shared_ptr<FieldEventPayloadValue> clone() const override { return checkpoint_make_shared<Payload>(*this); }
  void visit_sources(FieldEventSourceSink &sink) const override { if (value && value->command_result) sink.source(*value->command_result); }
};
Payload &typed(FieldEventPayloads &values) {
  auto *result = dynamic_cast<Payload *>(&values.ensure(4));
  if (!result) throw std::logic_error("FieldEventPayloadTypeMismatch");
  return *result;
}
const Payload *typed(const FieldEventPayloads &values) {
  const auto *base = values.find(4);
  if (!base) return nullptr;
  const auto *result = dynamic_cast<const Payload *>(base);
  if (!result) throw std::logic_error("FieldEventPayloadTypeMismatch");
  return result;
}
template <class C> Status fields(C &c, FieldMissionApplication &v) {
  using field_event_fields::field;
  field(c, "mission", v.mission);
  field(c, "mission_revision", v.mission_revision);
  field(c, "ai_pursuit", v.ai_pursuit);
  field(c, "command_result", v.command_result);
  field(c, "ai_source_id", v.ai_source_id);
  field(c, "order_id", v.order_id);
  return c.status();
}
template<class C> Status codec(C &c, FieldEventPayloads &values) {
  // 写端空槽只借局部空 optional；不为输出偷偷修改已发布状态。
  std::optional<FieldMissionApplication> empty;
  auto *existing = typed(static_cast<const FieldEventPayloads &>(values));
  auto &value = c.reading() ? typed(values).value
                          : (existing ? const_cast<std::optional<FieldMissionApplication> &>(existing->value) : empty);
  return c.object("mission_application", [&](auto &child) {
    return child.optional("optional", value, [](auto &record, auto &row) { return fields(record,row); });
  });
}
} // namespace
std::optional<FieldMissionApplication> &field_event_mission_application(FieldBattleEvent &event) { return typed(event.payloads).value; }
const std::optional<FieldMissionApplication> &field_event_mission_application(const FieldBattleEvent &event) {
  static const std::optional<FieldMissionApplication> empty;
  const auto *p = typed(event.payloads); return p ? p->value : empty;
}
const FieldEventPayloadRegistration &field_event_mission_application_registration() {
  static const FieldEventPayloadRegistration value{
    4, "mission_application", -1,
    []() -> std::shared_ptr<FieldEventPayloadValue> { return checkpoint_make_shared<Payload>(); },
    &codec<CodecVisitor>, &codec<ActorFoundationJsonVisitor>, &codec<ActorFoundationProjectionArchive>};
  return value;
}
} // namespace hanmo::resident
