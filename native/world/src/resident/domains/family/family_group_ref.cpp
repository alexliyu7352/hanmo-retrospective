#include "family_group_ref.hpp"
#include "family_group_ref_codec_fields.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
namespace hanmo::resident {
Status validate_family_group_ref_shape(const FamilyGroupRef&r){
 if(const auto*clan=std::get_if<FamilyClanRef>(&r))return clan->clan_id.empty()?common_failure("FamilyClanIdentityRequired",{},ErrorKind::internal):common_success();
 const auto*branch=std::get_if<FamilyBranchRef>(&r);return !branch||branch->clan_id.empty()||branch->branch_id.empty()?common_failure("FamilyBranchIdentityRequired",{},ErrorKind::internal):common_success();
}
bool same_family_group_ref(const FamilyGroupRef&a,const FamilyGroupRef&b)noexcept{
 if(a.index()!=b.index())return false;
 if(const auto*x=std::get_if<FamilyClanRef>(&a)){const auto*y=std::get_if<FamilyClanRef>(&b);return y&&x->clan_id==y->clan_id;}
 const auto*x=std::get_if<FamilyBranchRef>(&a);const auto*y=std::get_if<FamilyBranchRef>(&b);return x&&y&&x->clan_id==y->clan_id&&x->branch_id==y->branch_id;
}
Status visit_family_group_ref(CodecVisitor&c,FamilyGroupRef&r){return family_group_ref_codec::fields(c,r);}
Status emit_family_group_ref_fields(ProjectionWriter&w,const FamilyGroupRef&r){ActorFoundationProjectionArchive c(w);return family_group_ref_codec::fields(c,const_cast<FamilyGroupRef&>(r));}
Outcome<BoundaryValue> encode_family_group_ref(const FamilyGroupRef&r){BoundaryValue value;BoundaryProjectionWriter w(value);auto s=w.object("",[&](auto&x){return emit_family_group_ref_fields(x,r);});if(s.ok)s=w.finish();return {s.ok,std::move(value),s.error};}
Outcome<FamilyGroupRef> decode_family_group_ref(const BoundaryValue&input){FamilyGroupRef r;ActorFoundationJsonVisitor c(input);auto s=family_group_ref_codec::fields(c,r);if(s.ok)s=c.finish();return {s.ok,std::move(r),s.error};}
}
