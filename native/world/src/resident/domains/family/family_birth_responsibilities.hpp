// 生育与有限照护责任的当前/事实前配对纯读；原私产继受不能忽略尚未完成的本人承诺。
#pragma once
#include "family_birth.hpp"
namespace hanmo::resident {
struct BirthCareResponsibilities { WorldIdentity source_identity;StableId actor_id;std::vector<StableId> pending_acceptance_ids; };
// 前置为同一候选完整FamilyBirth历史已验证。只认双方实际接受后的责任，收到提议不构成负债。
Outcome<BirthCareResponsibilities> read_birth_care_responsibilities(const CommonStore&,const FamilyBirthState&,const StableId& actor_id);
// 消费方先核真实继产payload.tick；严格要求family_inheritance:/family_inheritance_settled原Common头。
// 双方已取消的未出生安排或有限照护闭合，只有原终态先于本边界才解除这份明确责任。
Outcome<BirthCareResponsibilities> read_birth_care_responsibilities_before_inheritance(const CommonStore&,const FamilyBirthState&,const StableId& actor_id,Integer tick,const StableId& inheritance_fact_id);
}
