// 军议的确定性源读取；原人物、真实编成和原生成格之外没有后备输入。
#include "field_council_sources.hpp"
#include "field_ability_sources.hpp"
#include "field_deployment.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/domains/organization/organization_model.hpp"
#include "resident/domains/social/social_model.hpp"
#include "resident/model/dynamic_contract.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <limits>
#include <set>
namespace hanmo::resident {
namespace {
using Wide = __int128_t;
// 只拒绝缺失或损坏的真实来源，不能把缺将领当作属性全零的参与者。
Outcome<FieldCouncilSources> invalid(const StableId &path) {
  return {false, {}, common_failure("FieldCouncilSourceRequired", path).error};
}
// 此原目录没有秩禄字段；仅用户表的明确职名与当前作者已有复合职名可映射。
// 不做子串猜测；未覆盖的新职名必须另交用户定准。
std::optional<Integer> original_office_rank(const StableId &title) {
  if (title == "将军" || title == "中郎将")
    return 5;
  if (title == "校尉" || title == "统军校尉")
    return 4;
  if (title == "都尉" || title == "别部司马" || title == "司马" ||
      title == "领军司马")
    return 3;
  if (title == "军候" || title == "参军")
    return 2;
  if (title == "屯长" || title == "队率")
    return 1;
  return {};
}
// 只读原实际任官，而不是角色称谓、兵员指挥范围或作者中尚空缺的职位。
Outcome<Integer> official_rank(const MilitaryState &military,
                               const CheckpointMap<StableId, StableId> &titles,
                               const StableId &actor,
                               const OrganizationState *organization) {
  Integer rank = 0;
  std::set<StableId> seen;
  for (const auto &office : military.offices) {
    if (!seen.insert(office.id).second)
      return {
          false,
          {},
          common_failure("FieldCouncilOfficeSourceMismatch", office.id).error};
    if (!office.enabled || office.holder_actor_id != actor)
      continue;
    const auto title = titles.find(office.id);
    if (title == titles.end())
      return {
          false,
          {},
          common_failure("FieldCouncilOfficeTitleRequired", office.id).error};
    const auto value = original_office_rank(title->second);
    if (!value)
      return {false,
              {},
              common_failure("FieldCouncilOfficeRankUnknown", office.id).error};
    rank = std::max(rank, *value);
  }
  // 原 Organization Office 只有业务授权字段，既不能直接当官阶，也不能
  // 由缺职名推导持有者无官职。涉及此类真实任职才拒，不挡没有该任职的参战者。
  if (organization)
    for (const auto &[id, office] : organization->offices.rows())
      if (office.status == OfficeStatus::active && office.holder_actor_id &&
          *office.holder_actor_id == actor)
        return {false,
                {},
                common_failure("FieldCouncilFormalOfficeEvidenceRequired", id)
                    .error};
  return {true, rank, {}};
}
// 合同规定的优先级；兵力比交叉乘，不经浮点或提前整除改变临界点。
FieldCouncilStance stance(const FieldCouncilOfficerSource &officer,
                          Integer own_fit, Integer enemy_fit) {
  if (officer.category == FieldTroopCategory::Cavalry && officer.cavalry >= 60)
    return FieldCouncilStance::Cavalry;
  if (Wide(officer.martial) >= Wide(officer.intellect) + 15)
    return FieldCouncilStance::Aggressive;
  if (Wide(officer.intellect) >= Wide(officer.martial) + 15)
    return FieldCouncilStance::Cunning;
  return Wide(own_fit) * 10 < Wide(enemy_fit) * 9 && officer.leadership >= 60
             ? FieldCouncilStance::Cautious
             : FieldCouncilStance::Steady;
}
// 分数保留原公式分母到比较时；等级阈值不受整数中间舍入影响。
Integer intel_level(const FieldCouncilSideSource &own,
                    const FieldCouncilSideSource &enemy) {
  Wide scout_twice = 0, conceal_fourth = 0;
  for (const auto &officer : own.officers)
    scout_twice =
        std::max(scout_twice, Wide(officer.intellect) + officer.tactics);
  if (own.cavalry_present)
    scout_twice += 20;
  for (const auto &officer : enemy.officers)
    if (officer.actor_id == enemy.commander_actor_id ||
        (enemy.strategist_actor_id &&
         officer.actor_id == *enemy.strategist_actor_id))
      conceal_fourth =
          std::max(conceal_fourth, Wide(officer.intellect) + officer.tactics);
  const Wide den = Wide(4) * enemy.deploy_cells;
  const Wide difference =
      (2 * scout_twice - 100 - conceal_fourth) * enemy.deploy_cells -
      Wide(120) * enemy.concealed_cells;
  return difference < -10 * den  ? 0
         : difference < 10 * den ? 1
         : difference < 30 * den ? 2
                                 : 3;
}
} // namespace
Outcome<FieldCouncilSources> read_field_council_sources(
    const CommonStore &common, const FieldBattleSession &session,
    const GeneratedBattleField &field, const SocialState &social,
    const MilitaryState &military,
    const CheckpointMap<StableId, StableId> &office_titles,
    const OrganizationState *organization,
    const FieldBattleAbilityReadContext &abilities) {
  if (session.field_hash != field.field_hash || session.formations.empty() ||
      field.cells.empty())
    return invalid(session.battle_id);
  FieldCouncilSources result;
  std::set<StableId> all_officers;
  for (Integer side = 0; side < 2; ++side) {
    const auto &original = session.sides[side];
    auto &out = result.sides[side];
    if (original.side != side || original.deploy_zone.empty())
      return invalid(session.battle_id + "/side");
    auto original_zone = read_field_side_geometry(session, field, side);
    if (!original_zone.ok)
      return {false, {}, original_zone.error};
    out.side = side;
    out.commander_actor_id = original.commander_actor_id;
    if (original.deploy_zone.size() >
        std::size_t(std::numeric_limits<Integer>::max()))
      return invalid(session.battle_id + "/deploy_zone_size");
    out.deploy_cells = static_cast<Integer>(original.deploy_zone.size());
    for (const auto cell : original.deploy_zone) {
      if (cell >= field.cells.size())
        return invalid(session.battle_id + "/deploy_cell");
      const auto kind = field.cells[cell].terrain;
      out.concealed_cells +=
          kind == FieldTerrainKind::Forest || kind == FieldTerrainKind::Rough;
    }
    for (const auto &[id, formation] : session.formations) {
      if (formation.side != side)
        continue;
      // 军议沿真实参战名册；原 available 不是 action_id/行军忙碌。
      // 在军中或冻结 OnRoute 不使将领缺席，不调用要求空闲的社会互动可用性门。
      const auto *actor =
          common.state().actors.find(formation.commander_actor_id);
      if (id != formation.formation_id || !actor || !actor->alive ||
          !actor->available || formation.commander_lost_at ||
          !all_officers.insert(actor->id).second || formation.fit.empty() ||
          formation.fit.size() >
              std::size_t(std::numeric_limits<Integer>::max() - out.fit))
        return invalid(id);
      FieldCouncilOfficerSource officer;
      officer.actor_id = actor->id;
      officer.formation_id = id;
      officer.category = formation.category;
      officer.fit = static_cast<Integer>(formation.fit.size());
      // 军议热读同一真实 Growth 历史；contact 栈与零时命令使用各自原边界。
      // 不从今日 Actor.skills 倒填开战能力，也不以空 Growth 代替缺失源。
      const auto actual =
          read_field_actual_application_moment(common, abilities);
      if (!actual.ok)
        return {false, {}, actual.error};
      const auto statistics = read_field_officer_statistics(
          common, abilities, actor->id, actual.value.half_day_tick);
      if (!statistics.ok)
        return {false, {}, statistics.error};
      officer.leadership = statistics.value.attributes.leadership;
      officer.martial = statistics.value.attributes.martial;
      officer.intellect = statistics.value.attributes.intellect;
      officer.administration = statistics.value.attributes.administration;
      officer.charisma = statistics.value.attributes.charisma;
      officer.infantry = statistics.value.infantry;
      officer.cavalry = statistics.value.cavalry;
      officer.archery = statistics.value.archery;
      officer.tactics = statistics.value.tactics;
      // 唯一原社会函数本来对缺关系返回 0，方向固定主将 -> 发言者。
      officer.commander_trust =
          social_trust(social, out.commander_actor_id, actor->id);
      if (officer.commander_trust < 0 || officer.commander_trust > 100)
        return invalid(actor->id + "/trust");
      auto rank =
          official_rank(military, office_titles, actor->id, organization);
      if (!rank.ok)
        return {false, {}, rank.error};
      officer.official_rank = rank.value;
      out.fit += officer.fit;
      out.cavalry_present = out.cavalry_present ||
                            officer.category == FieldTroopCategory::Cavalry;
      out.officers.push_back(std::move(officer));
    }
    const auto commander = std::find_if(
        out.officers.begin(), out.officers.end(), [&](const auto &officer) {
          return officer.actor_id == out.commander_actor_id;
        });
    if (commander == out.officers.end() || out.fit <= 0)
      return invalid(out.commander_actor_id);
    const FieldCouncilOfficerSource *strategist = nullptr;
    for (const auto &officer : out.officers) {
      if (officer.actor_id == out.commander_actor_id)
        continue;
      // §6.3 的既定并列次序；不从官阶缺口或人物姓名发明第二顺序。
      if (!strategist ||
          Wide(officer.intellect) + officer.tactics >
              Wide(strategist->intellect) + strategist->tactics ||
          (Wide(officer.intellect) + officer.tactics ==
               Wide(strategist->intellect) + strategist->tactics &&
           officer.formation_id < strategist->formation_id))
        strategist = &officer;
    }
    if (strategist)
      out.strategist_actor_id = strategist->actor_id;
  }
  for (Integer side = 0; side < 2; ++side) {
    auto &own = result.sides[side];
    const auto &enemy = result.sides[1 - side];
    for (auto &officer : own.officers)
      officer.stance = stance(officer, own.fit, enemy.fit);
    auto &intel = result.intel[side];
    intel.level = intel_level(own, enemy);
    if (intel.level == 0) {
      // 人数区间向外取整，确保信息只不完整，不把真实人数排除在区间之外。
      const Wide low = Wide(enemy.fit) * 4 / 5;
      const Wide high = (Wide(enemy.fit) * 6 + 4) / 5;
      if (high > std::numeric_limits<Integer>::max())
        return invalid(session.battle_id + "/intel_estimate");
      intel.total_estimate = {static_cast<Integer>(low),
                              static_cast<Integer>(high)};
    } else {
      intel.total_estimate = {enemy.fit, enemy.fit};
      intel.formations.emplace();
      for (const auto &officer : enemy.officers)
        intel.formations->push_back(
            {officer.formation_id, officer.category, officer.fit});
      if (intel.level >= 2) {
        intel.commanders.emplace();
        for (const auto &officer : enemy.officers)
          intel.commanders->push_back(
              {officer.actor_id, officer.leadership, officer.martial,
               officer.intellect, officer.administration, officer.charisma});
      }
    }
  }
  return {true, std::move(result), {}};
}
} // namespace hanmo::resident
