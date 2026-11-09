#pragma once
#include "family_group_ref.hpp"
namespace hanmo::resident::family_group_ref_codec {
template<class C>Status fields(C&c,FamilyGroupRef&r){
 Integer tag=static_cast<Integer>(r.index());c.field("tag",tag);
 if(tag<0||tag>1)return c.reject("InvalidFamilyGroupRefTag");
 if(c.reading()){if(tag==0)r=FamilyClanRef{};else r=FamilyBranchRef{};}
 if(tag==0)c.field("clan_id",std::get<FamilyClanRef>(r).clan_id);
 else{auto&v=std::get<FamilyBranchRef>(r);c.field("clan_id",v.clan_id);c.field("branch_id",v.branch_id);}
 if(!c.status().ok)return c.status();auto valid=validate_family_group_ref_shape(r);return valid.ok?c.status():c.reject(valid.error.original.code);
}
}
