// 真实出生者的年龄来源后继；只借已完整验证的本候选Birth账，不放宽旧无Birth入口。
#pragma once
#include "actor_age_read.hpp"
namespace hanmo::resident {
struct FamilyBirthState;
// 原作者/Family创建仍走原口；亲生人物另外精确核出生载荷、实际创建头、生日和边界先后。
// 调用者须已核完整Family/Creation以及选中的Birth/Lineage身份；本口不递归World或Birth业务总验。
Outcome<ActorAgeRead> read_actor_age(const CommonStore&,const CommonCatalog&,const FamilyBirthState&,const StableId& actor);
Outcome<ActorAgeRead> read_actor_age_before_fact(const CommonStore&,const CommonCatalog&,const FamilyBirthState&,const StableId& actor,Integer tick,const StableId& actual_before_fact);
}
