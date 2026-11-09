// 唯一显式 Field 事件登记点；没有静态构造注册、扫描文件或输入驱动扩型。
#include "field_event_registry.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include <array>
#include <stdexcept>
namespace hanmo::resident {
const FieldEventPayloadRegistration &field_event_mission_transition_registration();
const FieldEventPayloadRegistration &field_event_navigation_observation_registration();
const FieldEventPayloadRegistration &field_event_motion_application_registration();
const FieldEventPayloadRegistration &field_event_mission_application_registration();
const FieldEventPayloadRegistration &field_event_contact_phase_from_registration();

// 原编号/公开标签/知情层级固定；新子型新增自己的具名 validator 后仅在此登记。
const FieldEventKindRegistration *find_field_event_kind(FieldBattleEventKind kind) noexcept {
  static constexpr std::array<FieldEventKindRegistration,22> entries{{
    {{0}, "contact", FieldEventExposure::public_event, nullptr},
    {{1}, "casualties", FieldEventExposure::public_event, nullptr},
    {{2}, "rout", FieldEventExposure::public_event, nullptr},
    {{3}, "rallied", FieldEventExposure::public_event, nullptr},
    {{4}, "commander_lost", FieldEventExposure::public_event, nullptr},
    {{5}, "order_arrived", FieldEventExposure::private_side, nullptr},
    {{6}, "entered", FieldEventExposure::public_event, nullptr},
    {{7}, "withdrawn", FieldEventExposure::public_event, nullptr},
    {{8}, "ended", FieldEventExposure::public_event, nullptr},
    {{9}, "council_adopted", FieldEventExposure::private_side, nullptr},
    {{10}, "request_granted", FieldEventExposure::private_side, nullptr},
    {{11}, "request_denied", FieldEventExposure::private_side, nullptr},
    {{12}, "tactic", FieldEventExposure::public_event, nullptr},
    {{13}, "stratagem", FieldEventExposure::public_event, nullptr},
    {{14}, "revealed", FieldEventExposure::public_event, nullptr},
    {{15}, nullptr, FieldEventExposure::internal, nullptr},
    {{16}, nullptr, FieldEventExposure::internal, nullptr},
    {{17}, nullptr, FieldEventExposure::internal, nullptr},
    {{18}, nullptr, FieldEventExposure::internal, nullptr},
    {{19}, nullptr, FieldEventExposure::internal, nullptr},
    {{20}, nullptr, FieldEventExposure::internal, nullptr},
    {{21}, nullptr, FieldEventExposure::internal, nullptr},
  }};
  static_assert([] {
    for (std::size_t i = 0; i < entries.size(); ++i) {
      const auto &entry = entries[i];
      if (entry.id.value < 0 || (i < 22 && entry.id.value != static_cast<Integer>(i)) ||
          (entry.id.value > 21 && !entry.validate_original) ||
          (entry.exposure != FieldEventExposure::internal && !entry.public_name)) return false;
      for (std::size_t j = 0; j < i; ++j) if (entries[j].id == entry.id) return false;
    }
    return true;
  }(), "FieldEventKindRegistrationMismatch");
  for (const auto &entry : entries) if (entry.id == kind) return &entry;
  return nullptr;
}
bool registered_field_event_kind(FieldBattleEventKind kind) noexcept { return find_field_event_kind(kind) != nullptr; }
Status validate_field_event_payload_layout(const FieldBattleEvent &event) {
  if (!registered_field_event_kind(event.kind)) return common_failure("InvalidFieldBattleTag");
  for (const auto *entry : field_event_payload_registry())
    if (entry->only_kind >= 0 && entry->only_kind != event.kind.value && event.payloads.find(entry->id))
      return common_failure("FieldEventPayloadKindMismatch");
  return common_success();
}
Status validate_field_event_extension_source(const FieldEventValidationContext &context, const FieldBattleEvent &event) {
  const auto *entry = find_field_event_kind(event.kind);
  if (!entry || !entry->validate_original)
    return common_failure("NOT_IMPLEMENTED", "field_battle/driver_event_source", ErrorKind::unsupported_capability);
  return entry->validate_original(context,event);
}
std::span<const FieldEventPayloadRegistration *const> field_event_payload_registry() {
  static const std::array<const FieldEventPayloadRegistration *, 5> entries{
    &field_event_mission_transition_registration(),
    &field_event_navigation_observation_registration(),
    &field_event_motion_application_registration(),
    &field_event_mission_application_registration(),
    &field_event_contact_phase_from_registration(),
};
  for (std::size_t i = 0; i < entries.size(); ++i) {
    const auto *v = entries[i];
    if (!v || v->id <= 0 || !v->wire_name || !*v->wire_name || !v->create ||
        !v->binary || !v->json || !v->projection || v->only_kind < -1 ||
        (i < 5 && (v->id != static_cast<Integer>(i + 1) || v->only_kind != -1)) ||
        (i >= 5 && (v->id <= 5 || v->only_kind <= 21 ||
                    !registered_field_event_kind(FieldBattleEventKind{v->only_kind}))))
      throw std::logic_error("FieldEventPayloadRegistrationRequired");
    for (std::size_t j = 0; j < i; ++j)
      if (entries[j]->id == v->id || std::string_view(entries[j]->wire_name) == v->wire_name)
        throw std::logic_error("FieldEventPayloadRegistrationRepeated");
  }
  return entries;
}
const FieldEventPayloadRegistration &require_field_event_payload_registration(Integer id) {
  for (const auto *entry : field_event_payload_registry()) if (entry->id == id) return *entry;
  throw std::logic_error("UnregisteredFieldEventPayload");
}
Status visit_field_event_payload_fields(CodecVisitor &archive, FieldBattleEventKind kind, FieldEventPayloads &values) {
  for (const auto *entry : field_event_payload_registry()) {
    if (!archive.status().ok) return archive.status();
    if (entry->only_kind >= 0 && entry->only_kind != kind.value) {
      if(values.find(entry->id))return archive.reject("FieldEventPayloadKindMismatch");
      continue;
    }
    auto result = entry->binary(archive, values);
    if (!result.ok) return result;
  }
  return archive.status();
}
Status visit_field_event_payload_fields(ActorFoundationJsonVisitor &archive, FieldBattleEventKind kind, FieldEventPayloads &values) {
  for (const auto *entry : field_event_payload_registry()) {
    if (!archive.status().ok) return archive.status();
    if (entry->only_kind >= 0 && entry->only_kind != kind.value) {
      if(values.find(entry->id))return archive.reject("FieldEventPayloadKindMismatch");
      continue;
    }
    auto result = entry->json(archive, values);
    if (!result.ok) return result;
  }
  return archive.status();
}
Status visit_field_event_payload_fields(ActorFoundationProjectionArchive &archive, FieldBattleEventKind kind, FieldEventPayloads &values) {
  for (const auto *entry : field_event_payload_registry()) {
    if (!archive.status().ok) return archive.status();
    if (entry->only_kind >= 0 && entry->only_kind != kind.value) {
      if(values.find(entry->id))return archive.reject("FieldEventPayloadKindMismatch");
      continue;
    }
    auto result = entry->projection(archive, values);
    if (!result.ok) return result;
  }
  return archive.status();
}
} // namespace hanmo::resident
