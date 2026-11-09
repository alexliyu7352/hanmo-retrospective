// 原Family completion在JSON里摊平；只接受同一永久照护来源的等价冷热承载，不改旧visitor。
#pragma once
#include "resident/kernel/command_types.hpp"
#include <set>
namespace hanmo::resident {
// 完整原care史先验后传精确本人、当时player和结束fact；该形状函数本身不授照护/控制权限。
inline Status validate_family_care_completion_shape(const ActionCompletion& completion,const StableId& caregiver,const StableId& player,const StableId& fact,bool success,const StableId& reason){
 const auto fail=[&](){return common_failure("FamilyCareCompletionShapeMismatch",fact);};const auto code=reason.empty()?std::optional<StableId>{}:std::optional<StableId>{reason};
 if(!completion.ok||(completion.completed&&!*completion.completed)||(completion.success&&*completion.success!=success)||(completion.code&&completion.code!=code)||completion.already_completed||completion.military||completion.task||completion.growth||!completion.military_field_order.empty()||!completion.family)return fail();
 const auto& effect=*completion.family;if(effect.success!=std::optional<bool>{success}||effect.code!=code||effect.actor_id!=std::optional<StableId>{caregiver}||effect.player_actor_id!=std::optional<StableId>{player}||effect.fact_id!=std::optional<StableId>{fact}||effect.already_completed||effect.heir_actor_id)return fail();
 std::set<StableId> keys{"ok","success","actor_id","fact_id","player_actor_id"};if(code)keys.insert("code");const std::set<StableId> actual(effect.field_order.begin(),effect.field_order.end());if(!effect.field_order.empty()&&(actual!=keys||actual.size()!=effect.field_order.size()))return fail();
 // 原read_completion的Family分支无外层字段顺序；热原生也不写本层顺序，拒藏入二次承载。
 if(!completion.field_order.empty())return fail();return common_success();
}
}
