// 原军事供给/伤亡的人物HP与疲劳原因；只读业务真账，不持第二份条件账或授权任意人物写。
#pragma once
#include "military_roster_history.hpp"
namespace hanmo::resident {
enum class MilitaryActorConditionKind { supply,casualty };
enum class MilitaryActorConditionOperation { fatigue_add_four,hp_dead_zero,hp_wounded_cap_seventy };
struct MilitaryActorConditionRule {
 StableId actor_id;
 MilitaryActorConditionOperation operation=MilitaryActorConditionOperation::fatigue_add_four;
 // 原mouth_ids或actor_changes中的实际序号；滤掉token/非条件写后仍保原相对顺序。
 Integer source_index=0;
};
struct MilitaryActorConditionSource {
 WorldIdentity source_identity;
 StableId actual_fact_id;
 MilitaryActorConditionKind kind=MilitaryActorConditionKind::supply;
 Integer tick=0;
 std::vector<MilitaryActorConditionRule> actors;
};
struct MilitaryActorConditionValues {std::optional<Integer> hp,fatigue;};
// 调用前须完整验证原Military供给/名册/军战与Naval战果史。本口只再核唯一payload、
// canonical实际头/时钟/原序，不重入Growth/World，也不以今天的HP、疲劳、alive补历史。
// 足额Supply或仅俘获/退离的Casualty合法返回空actors，不伪造人物条件效应。
Outcome<MilitaryActorConditionSource> read_military_actor_condition_source(
 const CommonStore&,const MilitaryState&,const StableId& actual_fact_id);
// 严格preclock原from→from+1范围；旧source可读，但任何真实头必须已落同候选。
Outcome<MilitaryActorConditionSource> read_military_actor_condition_source(
 const CommonStore&,const MilitaryState&,const StableId& actual_fact_id,const DynamicStepInput&);
// 按Common原头序反向覆盖全部非空写源；该顺序只标识来源，不冒实际Actor写入顺序。
// Land先造casualty再批量写人，Naval先写人后造头；真实执行顺序仍由Common capture拥有。
Outcome<std::vector<MilitaryActorConditionSource>> read_military_actor_condition_sources(
 const CommonStore&,const MilitaryState&);
Outcome<std::vector<MilitaryActorConditionSource>> read_military_actor_condition_sources(
 const CommonStore&,const MilitaryState&,const DynamicStepInput&);
// 原三个显式ActorMutation数学，presence保留封顶同值写；before只能来自真实执行或完整条件重放。
Outcome<MilitaryActorConditionValues> apply_military_actor_condition_rule(
 MilitaryActorConditionOperation,Integer hp_before,Integer fatigue_before);
}
