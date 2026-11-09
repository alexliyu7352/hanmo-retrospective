// contact_phase_from 原强类型载荷独立拥有、复制、来源出口和三种原 codec 回调。
#include "field_event_contact_phase_from.hpp"
#include "field_event_typed_codec.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include <stdexcept>
namespace hanmo::resident {
namespace {
class Payload final : public FieldEventPayloadValue {
public:
  std::optional<FieldBattleTime> value;
  Integer type_id() const noexcept override { return field_event_contact_phase_from_payload_id; }
  std::shared_ptr<FieldEventPayloadValue> clone() const override { return checkpoint_make_shared<Payload>(*this); }
  void visit_sources(FieldEventSourceSink &sink) const override { (void)sink; }
};
Payload &typed(FieldEventPayloads &values) {
  auto *result = dynamic_cast<Payload *>(&values.ensure(5));
  if (!result) throw std::logic_error("FieldEventPayloadTypeMismatch");
  return *result;
}
const Payload *typed(const FieldEventPayloads &values) {
  const auto *base = values.find(5);
  if (!base) return nullptr;
  const auto *result = dynamic_cast<const Payload *>(base);
  if (!result) throw std::logic_error("FieldEventPayloadTypeMismatch");
  return result;
}
template<class C> Status fields(C &c, FieldBattleTime &v) { return field_event_original_value(c,v); }
template<class C> Status codec(C &c, FieldEventPayloads &values) {
  // 写端空槽只借局部空 optional；不为输出偷偷修改已发布状态。
  std::optional<FieldBattleTime> empty;
  auto *existing = typed(static_cast<const FieldEventPayloads &>(values));
  auto &value = c.reading() ? typed(values).value
                          : (existing ? const_cast<std::optional<FieldBattleTime> &>(existing->value) : empty);
  return c.object("contact_phase_from", [&](auto &child) {
    return child.optional("optional", value, [](auto &record, auto &row) { return fields(record,row); });
  });
}
} // namespace
std::optional<FieldBattleTime> &field_event_contact_phase_from(FieldBattleEvent &event) { return typed(event.payloads).value; }
const std::optional<FieldBattleTime> &field_event_contact_phase_from(const FieldBattleEvent &event) {
  static const std::optional<FieldBattleTime> empty;
  const auto *p = typed(event.payloads); return p ? p->value : empty;
}
const FieldEventPayloadRegistration &field_event_contact_phase_from_registration() {
  static const FieldEventPayloadRegistration value{
    5, "contact_phase_from", -1,
    []() -> std::shared_ptr<FieldEventPayloadValue> { return checkpoint_make_shared<Payload>(); },
    &codec<CodecVisitor>, &codec<ActorFoundationJsonVisitor>, &codec<ActorFoundationProjectionArchive>};
  return value;
}
} // namespace hanmo::resident
