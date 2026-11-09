// 恢复持久叶沿现有 DTO；未提供 resource_sites 根、来源或采集实现。
#pragma once
#include "resource_site_recovery.hpp"
namespace hanmo::resident {
class ProjectionWriter;
class CodecVisitor;
struct BoundaryValue;
#define HANMO_RESOURCE_CODEC(Name, Type)                                       \
  Outcome<Type> decode_##Name(const BoundaryValue &);                          \
  Outcome<BoundaryValue> encode_##Name(const Type &);                          \
  Status emit_##Name##_fields(ProjectionWriter &, const Type &);               \
  Status visit_##Name(CodecVisitor &, Type &);
HANMO_RESOURCE_CODEC(resource_recovery_rule, ResourceRecoveryRule)
HANMO_RESOURCE_CODEC(resource_depletion_modifier, ResourceDepletionModifier)
HANMO_RESOURCE_CODEC(resource_site_recovery_state, ResourceSiteState)
HANMO_RESOURCE_CODEC(resource_recovery_receipt, ResourceRecoveryReceipt)
#undef HANMO_RESOURCE_CODEC
} // namespace hanmo::resident
