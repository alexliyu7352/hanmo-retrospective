// Explicit exact /2 recovery leaf codec. Full ResourceSite owning root/source validation remains separate.
#pragma once
#include "resource_site_recovery2.hpp"
namespace hanmo::resident {
class ProjectionWriter;
class CodecVisitor;
struct BoundaryValue;
#define HANMO_RESOURCE_CODEC(Name, Type)                                       \
  Outcome<Type> decode_##Name(const BoundaryValue &);                          \
  Outcome<BoundaryValue> encode_##Name(const Type &);                          \
  Status emit_##Name##_fields(ProjectionWriter &, const Type &);               \
  Status visit_##Name(CodecVisitor &, Type &);
HANMO_RESOURCE_CODEC(resource_site_recovery_state2, ResourceSiteRecoveryState2)
HANMO_RESOURCE_CODEC(resource_recovery_receipt2, ResourceRecoveryReceipt2)
#undef HANMO_RESOURCE_CODEC
} // namespace hanmo::resident
