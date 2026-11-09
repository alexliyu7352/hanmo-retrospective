// Original gather owner values; only the complete ResourceSite owns their section.
#pragma once
#include "resource_gather_model.hpp"
namespace hanmo::resident {
class ProjectionWriter;
class CodecVisitor;
struct BoundaryValue;
#define HANMO_RESOURCE_CODEC(Name, Type)                                       \
  Outcome<Type> decode_##Name(const BoundaryValue &);                          \
  Outcome<BoundaryValue> encode_##Name(const Type &);                          \
  Status emit_##Name##_fields(ProjectionWriter &, const Type &);               \
  Status visit_##Name(CodecVisitor &, Type &);
HANMO_RESOURCE_CODEC(resource_gather_owner_values, ResourceGatherOwnerState)
#undef HANMO_RESOURCE_CODEC
} // namespace hanmo::resident
