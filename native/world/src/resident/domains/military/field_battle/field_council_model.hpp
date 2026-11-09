// Field 公开军议值；命令/读结果借此类型，不依赖内部事件载荷清单。
#pragma once
#include "field_mission_model.hpp"
#include "resident/kernel/source_ref.hpp"
namespace hanmo::resident {
// 军议只保合同中的实参、献策、侦察与准驳，不另存可写人物属性。
enum class FieldCouncilStance {
  Cavalry,
  Aggressive,
  Cunning,
  Cautious,
  Steady
};
enum class FieldCouncilRole { Commander, Strategist, Officer };
enum class FieldCouncilLayout { Hold, Line, Flank, Ambush };
enum class FieldCouncilWave { First, Second, Reserve };
enum class FieldCouncilStratagemKind { Ambush, FeintStrength };
enum class FieldCouncilRequestedRole { Vanguard, Flank, Reserve, Guard };
enum class FieldCouncilRequestStatus { Pending, Granted, Denied };
// 定案的实际编队任务与波次；第二波任务由原编队等待字段持有，不能提前执行。
struct FieldCouncilAssignment {
  StableId formation_id{};
  FieldMission mission = FieldHold{};
  FieldCouncilWave wave = FieldCouncilWave::First;
  bool flank_followup = false;
};
// 军议仅能安排埋伏或疑兵；地点来自本场真实格，战中混乱不混入开战方案。
struct FieldCouncilStratagemPlan {
  FieldCouncilStratagemKind kind = FieldCouncilStratagemKind::Ambush;
  std::optional<StableId> formation_id{};
  std::optional<PointI> area_dm{};
  // Independent Council command borrows the original Common receipt.
  std::optional<SourceRef> command_result{};
  // Retain rejected attachment history; never spend points for this item.
  bool voided_at_adoption = false;
};
// 主将可修改的布阵/分队方案；只有定案才要求完整覆盖本侧名册。
struct FieldCouncilPlan {
  FieldCouncilLayout layout = FieldCouncilLayout::Hold;
  std::vector<FieldCouncilAssignment> assignments{};
  std::vector<FieldCouncilStratagemPlan> stratagems{};
};
// 本侧真实在场将领及军议角色；不新增人物性格或第二份属性。
struct FieldCouncilParticipant {
  StableId actor_id{};
  FieldCouncilRole role = FieldCouncilRole::Commander;
  StableId formation_id{};
  FieldCouncilStance stance = FieldCouncilStance::Cavalry;
};
// 具名发言者的建议和文字键；同模板去重与选案顺序由原来源叶裁定。
struct FieldCouncilProposal {
  StableId proposal_id{};
  StableId speaker_actor_id{};
  FieldCouncilStance stance = FieldCouncilStance::Cavalry;
  FieldCouncilPlan plan{};
  StableId text_key{};
  std::optional<SourceRef> command_result;
};
// 部将明确自请的四种职责，不把玩家未发言当作请求。
struct FieldCouncilRoleRequest {
  FieldCouncilRequestedRole role = FieldCouncilRequestedRole::Vanguard;
};
// 玩家指定本部的真实目标编队，不用任意点或旧棋格代替。
struct FieldCouncilTargetRequest {
  StableId target_formation_id{};
};
using FieldCouncilRequestValue =
    std::variant<FieldCouncilRoleRequest, FieldCouncilTargetRequest>;
// 实际自请及主将准驳；拒绝仍保存可显示原因，不修改 S4 关系。
struct FieldCouncilRequest {
  StableId actor_id{};
  FieldCouncilRequestValue value = FieldCouncilRoleRequest{};
  FieldCouncilRequestStatus status = FieldCouncilRequestStatus::Pending;
  StableId reason{};
  std::optional<SourceRef> command_result;
};
// 一级敌情只给实际编队类别和兵数，不夹带将领技能或计划。
struct FieldCouncilIntelFormation {
  StableId formation_id{};
  FieldTroopCategory category = FieldTroopCategory::Infantry;
  Integer fit{};
};
// 二级敌情只增加原五项将领属性，不暴露第三等级计划。
struct FieldCouncilIntelCommander {
  StableId actor_id{};
  Integer leadership{};
  Integer martial{};
  Integer intellect{};
  Integer administration{};
  Integer charisma{};
};
// 按侦察等级显式有无的敌情；敌案只有三级且对方真定案后才存在。
struct FieldCouncilIntelSource {
  Integer level{};
  std::array<Integer, 2> total_estimate{};
  std::optional<std::vector<FieldCouncilIntelFormation>> formations{};
  std::optional<std::vector<FieldCouncilIntelCommander>> commanders{};
  std::optional<FieldCouncilPlan> enemy_plan{};
};
} // namespace hanmo::resident
