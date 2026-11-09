// 托管原源与政策目录的成对字段编解码，始终由唯一商店域拥有。
#pragma once
#include "private_shop_household_custody.hpp"
namespace hanmo::resident {
class ProjectionWriter;
class CodecVisitor;
struct BoundaryValue;
#define HANMO_PRIVATE_SHOP_CUSTODY_CODEC(Name, Type)                           \
  Outcome<Type> decode_##Name(const BoundaryValue &);                          \
  Outcome<BoundaryValue> encode_##Name(const Type &);                          \
  Status emit_##Name##_fields(ProjectionWriter &, const Type &);               \
  Status visit_##Name(CodecVisitor &, Type &);
HANMO_PRIVATE_SHOP_CUSTODY_CODEC(private_shop_household_custody_values,
                                 PrivateShopHouseholdCustodyState)
HANMO_PRIVATE_SHOP_CUSTODY_CODEC(private_shop_household_custody_catalog_values,
                                 PrivateShopHouseholdCustodyCatalog)
#undef HANMO_PRIVATE_SHOP_CUSTODY_CODEC
} // namespace hanmo::resident
