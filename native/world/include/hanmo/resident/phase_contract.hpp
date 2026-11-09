// 原17阶段与提交尾顺序的唯一合同；没有对象时仍要由域检查空集合，不填成功空函数。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
enum class Phase {
    preflight,military_supply,siege_step,military_battle,battle_actor_changes,
    siege_battles_finished,estate_production,contact_detect,actions_step,
    contact_install,social_step,siege_results,campaign_step,release_invalid_commands,
    clock_commit,military_return_complete,family_complete
};
inline constexpr std::array<Phase,17> phase_order={
    Phase::preflight,Phase::military_supply,Phase::siege_step,Phase::military_battle,
    Phase::battle_actor_changes,Phase::siege_battles_finished,Phase::estate_production,
    Phase::contact_detect,Phase::actions_step,Phase::contact_install,Phase::social_step,
    Phase::siege_results,Phase::campaign_step,Phase::release_invalid_commands,
    Phase::clock_commit,Phase::military_return_complete,Phase::family_complete};
// advance尾与submit尾分开；域阶段clock后仍不能直接发布权威世界。
enum class AdvanceTail { dialogue_resume,place_events,military_journeys,travel_schedule,military_schedule };
enum class CommitTail { release_invalid_commands,escort_observe,revision_result_history,domain_observers,validate_profile,publish };
// B拥有精确motion/contact类型及读版本；C仅在同phase消费，安装后不保留候选引用。
struct ContactBatch;
struct PhaseEffects;
struct PhaseContext {
    Integer from_tick=0,to_tick=0,phase_token=0;
    std::vector<StableId> hold,suspended_actors,family_completed;
    std::shared_ptr<const ContactBatch> contacts;
    std::shared_ptr<PhaseEffects> effects;
};
using PhaseHook=Status(*)(Txn&,const PreparedCatalog&,PhaseContext&);
using DomainValidator=Status(*)(const WorldRead&,const PreparedCatalog&);
// 静态注册真实函数；不从目录数据、JSON或GD Callable装载执行代码。
struct PhaseRegistration { Phase phase; const char* owner; PhaseHook run; };
} // 先单worker保语义；以后只在同一不可变输入上并行，并通过同一提交屏障。
