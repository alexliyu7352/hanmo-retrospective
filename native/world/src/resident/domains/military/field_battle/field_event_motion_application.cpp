// motion_application 原强类型载荷独立拥有、复制、来源出口和三种原 codec 回调。
#include "field_event_motion_application.hpp"
#include "field_event_typed_codec.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include <stdexcept>
namespace hanmo::resident {
namespace {
class Payload final : public FieldEventPayloadValue {
public:
  std::optional<FieldMotionApplication> value;
  Integer type_id() const noexcept override { return field_event_motion_application_payload_id; }
  std::shared_ptr<FieldEventPayloadValue> clone() const override { return checkpoint_make_shared<Payload>(*this); }
  void visit_sources(FieldEventSourceSink &sink) const override { (void)sink; }
};
Payload &typed(FieldEventPayloads &values) {
  auto *result = dynamic_cast<Payload *>(&values.ensure(3));
  if (!result) throw std::logic_error("FieldEventPayloadTypeMismatch");
  return *result;
}
const Payload *typed(const FieldEventPayloads &values) {
  const auto *base = values.find(3);
  if (!base) return nullptr;
  const auto *result = dynamic_cast<const Payload *>(base);
  if (!result) throw std::logic_error("FieldEventPayloadTypeMismatch");
  return result;
}
template <class C> Status fields(C &c, FieldMotionApplication &v) {
  using field_event_fields::field;
  field(c, "evaluated_at", v.evaluated_at);
  field(c, "navigation_sequence", v.navigation_sequence);
  field(c, "previous_motion_sequence", v.previous_motion_sequence);
  field(c, "from_dm", v.from_dm);
  field(c, "to_dm", v.to_dm);
  field(c, "reached", v.reached);
  field(c, "blocked", v.blocked);
  return c.status();
}
template<class C> Status codec(C &c, FieldEventPayloads &values) {
  // 写端空槽只借局部空 optional；不为输出偷偷修改已发布状态。
  std::optional<FieldMotionApplication> empty;
  auto *existing = typed(static_cast<const FieldEventPayloads &>(values));
  auto &value = c.reading() ? typed(values).value
                          : (existing ? const_cast<std::optional<FieldMotionApplication> &>(existing->value) : empty);
  return c.object("motion_application", [&](auto &child) {
    return child.optional("optional", value, [](auto &record, auto &row) { return fields(record,row); });
  });
}
} // namespace
std::optional<FieldMotionApplication> &field_event_motion_application(FieldBattleEvent &event) { return typed(event.payloads).value; }
const std::optional<FieldMotionApplication> &field_event_motion_application(const FieldBattleEvent &event) {
  static const std::optional<FieldMotionApplication> empty;
  const auto *p = typed(event.payloads); return p ? p->value : empty;
}
const FieldEventPayloadRegistration &field_event_motion_application_registration() {
  static const FieldEventPayloadRegistration value{
    3, "motion_application", -1,
    []() -> std::shared_ptr<FieldEventPayloadValue> { return checkpoint_make_shared<Payload>(); },
    &codec<CodecVisitor>, &codec<ActorFoundationJsonVisitor>, &codec<ActorFoundationProjectionArchive>};
  return value;
}
} // namespace hanmo::resident
