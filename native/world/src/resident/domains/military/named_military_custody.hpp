// 具名看管只读投影：沿原俘获、返营和 BW07 名单，不建立第二份人身或粮食账。
#pragma once
#include "resident/domains/military/military_refs.hpp"
#include "resident/kernel/source_ref.hpp"
namespace hanmo::resident {
class CommonStore;
struct FieldBattleWorldState;
struct NavalState;
class MilitaryFormationSourceRead;
class CurrentMomentRead;
class FieldSegmentApplicationRead;
class NamedMilitaryCustodyReleaseRead;
// 调用者借同一个 World 的原根；模块选择从原内容身份读取，不能以空指针冒未选。
struct NamedMilitaryCustodyContext {
  const CommonStore &common;
  const MilitaryState &military;
  const MilitaryCatalog &catalog;
  const CatalogIdentity &identity;
  const FieldBattleWorldState *field = nullptr;
  const NavalState *naval = nullptr;
  const MilitaryFormationSourceRead *formations = nullptr;
  const CurrentMomentRead *clock = nullptr;
  const MilitaryAccess *access = nullptr;
  const FieldSegmentApplicationRead *field_applications = nullptr;
  const NamedMilitaryCustodyReleaseRead *releases = nullptr;
};
// location_binding 是原 InArmy/Settlement 绑定，不猜战场内的俘虏坐标。
// 当前看管来源可为原俘获、真实舟战/占领转交或原返营；此类型不承诺过去 A 的状态。
struct NamedMilitaryCustodyView {
  StableId actor_id, captor_army_id;
  std::optional<StableId> source_unit_id, garrison_id;
  Location location_binding = SettlementLocation{};
  SourceRef capture_source, current_custody_source;
  Integer capture_tick = 0;
};
// 仅借同 World 原收据和人物变化；转交已有俘虏不构成新的俘获起点。
struct OriginalNamedMilitaryCapture {
  const MilitaryCasualtyFact *receipt = nullptr;
  const MilitaryActorChange *change = nullptr;
  const ArmyRecord *captured_army = nullptr;
  SourceRef army_formation_source;
  Integer army_formation_schema = 1;
  bool transfers_existing_custody = false;
  std::optional<WorldMoment> applied_at;
  SourceRef application_source;
  Integer actor_ordinal = -1, application_schema = 1;
};
Outcome<OriginalNamedMilitaryCapture> read_original_named_military_capture(
    const NamedMilitaryCustodyContext &, const SourceRef &,
    const StableId &actor_id);
// 仅真实没有原看管来源和占用才返回 null；已选 Field 缺根、坏回链一律显式失败。
Outcome<std::optional<NamedMilitaryCustodyView>> read_current_named_military_custody(
    const NamedMilitaryCustodyContext &, const StableId &actor_id);
// 覆盖所有原具名 captured/prisoner 行；匿名人口 token 仍由原人口军务验证。
Status validate_current_named_military_custody(const NamedMilitaryCustodyContext &);
// 只供中央对真实命令 variant 作有限分类，不是公开请求字段，也不接受 allow 开关。
// 唯读查询不走世界变更门；现有控制命令尚非 K3-2 通用切人，不在这里预先放行。
enum class NamedMilitaryCustodyCommandKind {
  half_day_wait,
  co_located_dialogue,
  other_world_change
};
// 返回原看管来源供中央区别普通在军权限门；不替代 alive、时间或互动空间校验。
// half_day_wait 必须由原等待路径落实半日、原地且不工作；不能借任意 Advance 上界。
// 对话衍生的旅行、交易、离开等另按实际动作重新入门，不能继承对话外壳的许可。
Outcome<std::optional<NamedMilitaryCustodyView>>
read_named_military_custody_command_gate(
    const NamedMilitaryCustodyContext &, const StableId &actor_id,
    NamedMilitaryCustodyCommandKind);
// 旅行、交易及其它未列明的世界变更入口采用同一 ActorInCustody 拒绝码。
Status require_named_military_actor_not_in_custody(
    const NamedMilitaryCustodyContext &, const StableId &actor_id);
} // namespace hanmo::resident
