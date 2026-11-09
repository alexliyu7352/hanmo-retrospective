// A/B只借同候选C域的真实占用与军团名册；复制世界后必须重建这些短寿命适配器。
#pragma once
#include "interaction_model.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/domains/military/battle_world.hpp"
#include "resident/model/common_model.hpp"
#include <algorithm>
namespace hanmo::resident {
class MilitaryMembership final : public MembershipResolver {
public:
    explicit MilitaryMembership(const MilitaryState& state):state_(state){}
    const std::vector<StableId>* active_army_members(const StableId& actor) const override {auto a=active_army_for_actor(state_,actor);return a?&a->member_actor_ids:nullptr;}
    const StableId* army_leader(const StableId& actor) const override {auto a=active_army_for_actor(state_,actor);return a?&a->commander_actor_id:nullptr;}
private: const MilitaryState& state_;
};
class InteractionActivity final : public ActivityRead {
public:
    InteractionActivity(const MilitaryState& military,const InteractionState& interaction):military_(military),interaction_(interaction){}
    // 已验证profile的battles为空；非空临时候选仍按实际参与名单回答，最终能力门会拒绝提交。
    bool in_open_battle(const StableId& actor) const override {return military_actor_in_battle(military_,actor);}
    // 对话owner和provider都占用；管理命令仅排除本人当前会话，不放行别人的谈话。
    bool in_interaction(const StableId& actor,const StableId& allowed_speaker={}) const override {for(auto& session:interaction_.interactions)if(session.actor_id!=allowed_speaker&&(session.actor_id==actor||session.context.provider_actor_id==actor))return true;return false;}
private: const MilitaryState& military_;const InteractionState& interaction_;
};
}
