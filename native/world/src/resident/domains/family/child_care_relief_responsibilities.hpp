// 本人原援助责任只读，供私人继产当前/事实前成对消费；死亡和今天已解除预约不能倒填过去结清。
#pragma once
#include "child_care_relief.hpp"
namespace hanmo::resident {
struct ChildCareReliefResponsibilities {WorldIdentity source_identity;StableId actor_id;std::vector<StableId> pending_acceptance_ids;};
// 同候选完整本域/Birth/Life史先验；本读口不重新进入World或以私粮零余额当免责。
Outcome<ChildCareReliefResponsibilities> read_child_care_relief_responsibilities(const CommonStore&,const ChildCareReliefState&,const StableId& actor_id);
Outcome<ChildCareReliefResponsibilities> read_child_care_relief_responsibilities_before_inheritance(const CommonStore&,const ChildCareReliefState&,const StableId& actor_id,Integer tick,const StableId& actual_inheritance_fact);
}
