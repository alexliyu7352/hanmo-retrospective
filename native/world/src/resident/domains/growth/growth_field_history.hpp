// 新野战只借同一 Growth 原永久历史内核；不启旧 rout，也不构造第二 XP 账。
#pragma once
#include "growth_naval_history.hpp"
namespace hanmo::resident {
struct FactRecord;
class CurrentMomentRead;
class OriginalWorldReadBoundary;
struct WorldMoment;
using GrowthFieldHistoryContext = GrowthNavalHistoryContext;
using GrowthFieldSkillObservation = GrowthNavalSkillObservation;
// Field owner 先验证实际 session/start/entry/council采用/stratagem采用
// 正文及角色、半日、阶段；这里不授战斗权。 本域再核原 Common
// 头地址/有限种类，并按真实阶段截断原技艺贡献。
// Internal finite boundary, not persisted wire. Commander change must be the
// actual Field owner-verified local Lost/deputy source; it is not a DeathRef.
enum class GrowthFieldBoundaryKind {
  npc_contact_session,
  player_started,
  actor_entered,
  council_adopted,
  stratagem_adopted,
  commander_changed,
  npc_started,
  npc_council_adopted,
  actor_entered_at_contact
};
// 唯一 contact_install 栈：actions_step 后、clock_commit 前。phase 必为当前
// from→to。 四种 skill 仅
// cavalry/infantry/archery/tactics；每次读仍验证该技能完整原净增链。
Outcome<GrowthFieldSkillObservation> read_growth_field_skill_at_contact(
    const CommonStore &, const GrowthFieldHistoryContext &, const GrowthState &,
    const StableId &actor, const StableId &skill, const DynamicStepInput &);
// 非持久当前预览：只读同 World 原 Common 全前缀，必须有实际 S0 边界证明。
// 可观察真实分数 M，不用 phase.to 伪造已结算课时，也不制造采用头。
// 不能把返回值当历史参战快照；真正采用仍用 before_fact 原入口。
Outcome<GrowthFieldSkillObservation> read_growth_field_skill_current_preview(
    const CommonStore &, const GrowthFieldHistoryContext &, const GrowthState &,
    const StableId &actor, const StableId &skill,
    const OriginalWorldReadBoundary &, const CurrentMomentRead &);
// 已提交宏观边界的当前零时应用。尚有可能同半日正向迟答/取消的原课必须拒绝，
// 不能热捕获成功后在冷读时因缺真实 XP 采用头而追认另一前缀。
Outcome<GrowthFieldSkillObservation> read_growth_field_skill_before_zero_time(
    const CommonStore &, const GrowthFieldHistoryContext &, const GrowthState &,
    const StableId &actor, const StableId &skill);
// actual_header 必为同 Common 原行。NPC session/显式 npc_started 沿
// exact contact_install 原阶段及实际fraction M，npc_started
// 必为真正军议采用后的 field_battle_started；npc_council_adopted
// 是同原安装阶段的真 Adopted， 不把 Speak 当 Adopt。玩家 start 与
// 后到entry、军议采用、施计采用、真实换将均不借旧land_entry同tick先后；同tick正净增缺源仍拒。
// 调用者先验 header↔真实 Field 正文/observed_tick，禁止借同名副本或当前 XP
// 倒填。source_contact_phase只说明本次NPC头正在原安装栈；
// actual_current_growth_phase独立给今日全XP账的真实上界，旧头仍按旧边界截取。
// actor_entered_at_contact 仅真援军入场头：S2先核正文与同World S0 clock。
// 必借实际M/clock；当前phase允许[from,to]内分数M，历史仍按原头截断。
// 它保持field_zero_time净增门，不采用NPC contact的同tick自动课时推定。
// 两个explicit NPC kinds同样必借actual M/clock，可为开局0+fraction；
// 仍按原contact净增规则。旧npc_contact_session保原to-only。
Outcome<GrowthFieldSkillObservation> read_growth_field_skill_before_fact(
    const CommonStore &, const GrowthFieldHistoryContext &, const GrowthState &,
    const StableId &actor, const StableId &skill, Integer observed_tick,
    const FactRecord &actual_header, GrowthFieldBoundaryKind,
    const DynamicStepInput *source_contact_phase = nullptr,
    const DynamicStepInput *actual_current_growth_phase = nullptr,
    const WorldMoment *actual_source_moment = nullptr,
    const CurrentMomentRead *actual_clock = nullptr);
} // namespace hanmo::resident
