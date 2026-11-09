// 个人战与军战的原伤因只读合同；不拥有人物伤势或产生第二份伤病账。
#pragma once
#include "military_roster_history.hpp"
#include "resident/domains/personal/personal_model.hpp"
namespace hanmo::resident {
// kind对应原伤势写入语义：military_wound取max(旧值,1)，personal_defeat取min(3,旧值+magnitude)。
struct MilitaryActorInjuryCause {
 WorldIdentity identity;
 StableId actor_id,exact_fact_id,kind;
 Integer tick=0,magnitude=0;
 // 仅个人终局填真实新分配result实例；battle头在开战分配、终局追加，不能用其旧号当施伤时序。
 // 此为瞬态来源字段，无新增保存账；军伤留空，沿原即时casualty头。
 StableId completion_id;
};
// 调用前已验证同候选Military/Naval及Personal历史；本口另核直接原头、终局和原作者条款。
// 返回全部非零伤因并严格按Common事实顺序排列；今天injury或alive不用于填补过去。
Outcome<std::vector<MilitaryActorInjuryCause>> query_military_actor_injury_causes(
 const CommonStore&,const MilitaryState&,const PersonalState&,const PersonalCatalog&,const StableId& actor_id);
// preclock只允许原from→from+1候选的真实伤亡；不会把任意未来tick变成公共许可。
Outcome<std::vector<MilitaryActorInjuryCause>> query_military_actor_injury_causes(
 const CommonStore&,const MilitaryState&,const PersonalState&,const PersonalCatalog&,const StableId& actor_id,const DynamicStepInput&);
}
