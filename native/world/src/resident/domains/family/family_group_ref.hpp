// P24.3 / V12.2: addresses the actual family group once. A group address alone
// proves neither membership nor a known deed, recognition, authority or benefit.
#pragma once
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {
struct FamilyClanRef {StableId clan_id;};
struct FamilyBranchRef {StableId clan_id,branch_id;};
using FamilyGroupRef=std::variant<FamilyClanRef,FamilyBranchRef>;
Status validate_family_group_ref_shape(const FamilyGroupRef&);
bool same_family_group_ref(const FamilyGroupRef&,const FamilyGroupRef&)noexcept;
class ProjectionWriter;
Status visit_family_group_ref(CodecVisitor&,FamilyGroupRef&);
Status emit_family_group_ref_fields(ProjectionWriter&,const FamilyGroupRef&);
Outcome<BoundaryValue> encode_family_group_ref(const FamilyGroupRef&);
Outcome<FamilyGroupRef> decode_family_group_ref(const BoundaryValue&);
}
