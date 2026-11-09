// 五项原人物属性只认Installed/真实创建；当前无业务属性写者，未知差额明确拒。
#pragma once
#include "growth_skill_origin.hpp"
namespace hanmo::resident {
struct GrowthAttributeOrigin {
 WorldIdentity source_identity;
 StableId actor_id,origin_kind,authored_actor_id,prototype_actor_id,before_fact_id;
 Integer created_tick=0,observed_tick=0;
 ActorAttributes attributes;
 std::vector<StableId> source_fact_ids;
};
// 完整同候选Family/Creation及启用Birth先验；不核年龄、生死、职务或功勋，不授任命权。
// 技能来源核仅复用原唯一身份链；属性来自真实原型或本人创建draft，并须与当前不可变属性相等。
Outcome<GrowthAttributeOrigin> read_growth_attribute_origin(const CommonStore&,const GrowthSkillOriginContext&,const StableId& actor);
// 调用域先核具名业务头及payload tick。本口仅stable真实Common边界，不开放任意future。
Outcome<GrowthAttributeOrigin> read_growth_attribute_origin_before_fact(const CommonStore&,const GrowthSkillOriginContext&,const StableId& actor,Integer observed_tick,const StableId& actual_fact);
}
