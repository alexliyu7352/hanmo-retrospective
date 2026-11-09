// 私有确定登记协议；只分派强类型 codec/clone，不取代原业务验证。
#pragma once
#include "field_event.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include <span>
namespace hanmo::resident {
struct CodecVisitor;
class ActorFoundationJsonVisitor;
struct FieldEventPayloadRegistration {
  Integer id;
  const char *wire_name;
  // -1 仅保旧五个无条件 optional；新子型只在自己事件编号出现时编码。
  Integer only_kind;
  std::shared_ptr<FieldEventPayloadValue> (*create)();
  Status (*binary)(CodecVisitor &, FieldEventPayloads &);
  Status (*json)(ActorFoundationJsonVisitor &, FieldEventPayloads &);
  Status (*projection)(ActorFoundationProjectionArchive &, FieldEventPayloads &);
};
std::span<const FieldEventPayloadRegistration *const> field_event_payload_registry();
const FieldEventPayloadRegistration &require_field_event_payload_registration(Integer);
// 扩展事件只能显式登记完整原上下文 validator；登记存在本身不判业务有效。
class CommonStore;
struct MilitaryState;
struct TravelCatalog;
struct FieldBattleCatalog;
struct FieldBattleWorldState;
struct FieldBattleSession;
struct GeneratedBattleField;
struct FieldBattleAbilityReadContext;
struct FieldEventValidationContext {
  const CommonStore &common;
  const MilitaryState &military;
  const TravelCatalog &travel;
  const FieldBattleCatalog &catalog;
  const FieldBattleWorldState &world;
  const FieldBattleSession &session;
  const GeneratedBattleField &field;
  const FieldBattleAbilityReadContext *abilities;
};
enum class FieldEventExposure { public_event, private_side, internal };
struct FieldEventKindRegistration {
  FieldBattleEventKind id;
  const char *public_name;
  FieldEventExposure exposure;
  Status (*validate_original)(const FieldEventValidationContext &, const FieldBattleEvent &);
};
const FieldEventKindRegistration *find_field_event_kind(FieldBattleEventKind) noexcept;
Status validate_field_event_payload_layout(const FieldBattleEvent &);
Status validate_field_event_extension_source(const FieldEventValidationContext &, const FieldBattleEvent &);
bool registered_field_event_kind(FieldBattleEventKind) noexcept;
Status visit_field_event_payload_fields(CodecVisitor &, FieldBattleEventKind, FieldEventPayloads &);
Status visit_field_event_payload_fields(ActorFoundationJsonVisitor &, FieldBattleEventKind, FieldEventPayloads &);
Status visit_field_event_payload_fields(ActorFoundationProjectionArchive &, FieldBattleEventKind, FieldEventPayloads &);
} // namespace hanmo::resident
