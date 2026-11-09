#include "resident/domains/military/military_model.hpp"
// 当前看管的来源只认实际 Common 因果链；不从 unavailable
// 反猜被俘，也不拿今天证明过去。
#include "field_battle/field_segment_settlement.hpp"
#include "field_battle/field_validation.hpp"
#include "military_formation_source.hpp"
#include "named_military_custody.hpp"
#include "named_military_custody_release_read.hpp"
#include "naval_casualties.hpp"
#include "naval_custody.hpp"
#include "occupation_rules.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <charconv>
#include <set>
namespace hanmo::resident {
namespace {
// 所有读失败保留具体缺源；没有“出错就自由”的回退分支。
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
void take(Status value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
}
template <class T>
const T *unique(const std::vector<T> &rows, const StableId &id) {
  const T *found = nullptr;
  for (const auto &row : rows)
    if (row.id == id) {
      if (found)
        fail("NamedMilitaryCustodyDuplicatePayload", id);
      found = &row;
    }
  return found;
}
bool has(const std::vector<StableId> &rows, const StableId &id) {
  return std::find(rows.begin(), rows.end(), id) != rows.end();
}
std::size_t position(const CommonStore &common, const SourceRef &source) {
  return take(read_actual_common_fact_position(common, source));
}
// 内容选择不是运行时缺省值；Field 已选而根缺席必须先失败。
void selection(const NamedMilitaryCustodyContext &context) {
  const auto field = take(selected_field_battle_version(context.identity));
  if (bool(field) != bool(context.field))
    fail("NamedMilitaryCustodyFieldRootSelectionMismatch");
  Integer naval = 0;
  bool seen = false;
  for (const auto &[name, text] : context.identity.module_versions)
    if (name == "military_naval") {
      if (seen)
        fail("NamedMilitaryCustodyNavalSelectionMismatch");
      seen = true;
      const auto parsed =
          std::from_chars(text.data(), text.data() + text.size(), naval);
      if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
          naval < 1 || naval > 2 || std::to_string(naval) != text)
        fail("NamedMilitaryCustodyNavalSelectionMismatch");
    }
  if (bool(naval) != bool(context.naval))
    fail("NamedMilitaryCustodyNavalRootSelectionMismatch");
  if (context.field) {
    if (!context.field_applications || !context.clock ||
        !context.field_applications->bound_to(context.common, context.military,
                                              *context.field))
      fail("NamedMilitaryCustodyFieldApplicationOwnerRequired");
    const auto actual = take(context.field_applications->read_current_moment());
    const auto now = take(context.clock->read_current_moment());
    if (take(compare_world_moments(actual, now)) != 0)
      fail("NamedMilitaryCustodyFieldApplicationClockMismatch");
    take(validate_field_segment_settlement_sources(
        context.common, context.military, *context.field,
        context.field_applications));
  }
}
// Field 直接读真正段父源；旧陆战与舟战继续各自原载荷链，不借一个假旧
// BattleRecord。
void capture_parent(const NamedMilitaryCustodyContext &context,
                    const MilitaryCasualtyFact &receipt) {
  const auto &common = context.common;
  const auto &military = context.military;
  const auto child =
      position(common, {"military", "military_casualties", receipt.id});
  const auto *head = common.state().facts.find(receipt.battle_id);
  if (!head || receipt.tick < 0)
    fail("NamedMilitaryCustodyCaptureParentRequired", receipt.id);
  const bool exact_field =
      head->domain == "military" && head->kind == "field_battle_segment";
  // Only the original Field application reader can admit an active S0
  // closing source beyond Common's opening tick. Other legacy parents retain
  // their original committed-tick gate; no numeric next-tick allowance.
  if (!exact_field && receipt.tick > common.tick())
    fail("NamedMilitaryCustodyCaptureParentRequired", receipt.id);
  const SourceRef parent{head->domain, head->kind, head->id};
  if (position(common, parent) >= child)
    fail("NamedMilitaryCustodyCaptureOrderMismatch", receipt.id);
  if (parent.domain == "military" && parent.kind == "field_battle_segment") {
    if (!context.field)
      fail("NamedMilitaryCustodyFieldRootRequired", receipt.id);
    const auto source = take(read_field_segment_settlement_source(
        common, military, *context.field, parent, context.field_applications));
    if (source.casualties != &receipt || !source.settlement ||
        source.settlement->applied_at.half_day_tick != receipt.tick)
      fail("NamedMilitaryCustodyCaptureParentMismatch", receipt.id);
  } else if (parent.domain == "military" && parent.kind == "military_battle") {
    const auto *fact = unique(military.battle_facts, parent.id);
    const auto *battle =
        fact ? unique(military.battles, fact->session_id) : nullptr;
    if (!fact || !battle || fact->tick != receipt.tick ||
        fact->phase != "completed" ||
        std::count(battle->round_receipt_ids.begin(),
                   battle->round_receipt_ids.end(), receipt.id) != 1)
      fail("NamedMilitaryCustodyCaptureParentMismatch", receipt.id);
  } else if (parent.domain == "military_naval" &&
             (parent.kind == "naval_round" || parent.kind == "naval_capture")) {
    if (!context.naval)
      fail("NamedMilitaryCustodyNavalRootRequired", receipt.id);
    take(validate_naval_casualty_receipt(common, military, *context.naval,
                                         receipt.id, common.tick()));
  } else {
    fail("NamedMilitaryCustodyCaptureParentRequired", receipt.id);
  }
}
} // namespace
Outcome<OriginalNamedMilitaryCapture>
read_original_named_military_capture(const NamedMilitaryCustodyContext &context,
                                     const SourceRef &source,
                                     const StableId &actor_id) {
  try {
    selection(context);
    if (source.domain != "military" || source.kind != "military_casualties" ||
        !context.common.actor(actor_id))
      fail("NamedMilitaryCustodyCaptureSourceRequired", source.id);
    const auto *receipt = unique(context.military.casualty_facts, source.id);
    if (!receipt)
      fail("NamedMilitaryCustodyCaptureSourceRequired", source.id);
    capture_parent(context, *receipt);
    const MilitaryActorChange *change = nullptr;
    for (const auto &row : receipt->actor_changes)
      if (row.actor_id == actor_id) {
        if (change || row.status != "captured")
          fail("NamedMilitaryCustodyCaptureActorMismatch", actor_id);
        change = &row;
      }
    if (!change || change->army_id.empty() || change->captor_army_id.empty() ||
        change->army_id == change->captor_army_id ||
        !unique(context.military.armies, change->army_id) ||
        !unique(context.military.armies, change->captor_army_id))
      fail("NamedMilitaryCustodyCaptureActorMismatch", actor_id);
    // 原军团的势力由真正形成事实锁定，不从人物多重会籍或初态势力挑选。
    const auto *captured_army =
        unique(context.military.armies, change->army_id);
    SourceRef formation_source;
    Integer formation_schema = 1;
    std::optional<WorldMoment> exact_formation_at;
    if (context.formations) {
      if (!context.clock)
        fail("NamedMilitaryCustodyActualClockRequired", captured_army->id);
      const auto now = take(context.clock->read_current_moment());
      const auto formation = take(read_original_military_formation(
          context.common.state(), context.military, *captured_army, now,
          context.formations));
      formation_source = formation.completed_source.source_ref;
      formation_schema = formation.completed_source.source_schema;
      if (formation_schema == 2)
        exact_formation_at = formation.completed_at;
      if (!formation.original_terms ||
          formation.original_terms->organization_id !=
              captured_army->organization_id ||
          formation.completed_at.half_day_tick > receipt->tick)
        fail("NamedMilitaryCustodyArmyFormationRequired", captured_army->id);
    } else {
      for (const auto &[module, version] : context.identity.module_versions)
        if (module == "military_action_time")
          fail("NamedMilitaryCustodyFormationReadRequired", captured_army->id);
      const auto *formation = unique(context.military.action_facts,
                                     captured_army->formation_source);
      const auto *terms =
          formation ? std::get_if<ArmyFormationTerms>(&formation->terms)
                    : nullptr;
      if (!formation || !terms || formation->command != "army_form" ||
          formation->phase != "completed" || !formation->success ||
          !formation->result ||
          formation->result->army_id != captured_army->id ||
          formation->actor_id != terms->actor_id ||
          terms->organization_id != captured_army->organization_id ||
          terms->order_id != captured_army->order_id ||
          formation->completed_tick < 0 ||
          formation->completed_tick > receipt->tick)
        fail("NamedMilitaryCustodyArmyFormationRequired", captured_army->id);
      formation_source = {"military", "military_action", formation->id};
    }
    // 真实 Common 顺序证明形成在先；粗 capture_tick 不充当精确世界时刻。
    if (position(context.common, formation_source) >=
        position(context.common, source))
      fail("NamedMilitaryCustodyArmyFormationRequired", captured_army->id);
    bool transferred = false;
    const auto *parent = context.common.state().facts.find(receipt->battle_id);
    if (parent->domain == "military_naval" && parent->kind == "naval_capture") {
      const auto *capture = unique(context.naval->battle.captures, parent->id);
      if (!capture)
        fail("NamedMilitaryCustodyCaptureParentRequired", parent->id);
      const auto fresh = std::count(capture->prisoner_ids.begin(),
                                    capture->prisoner_ids.end(), actor_id);
      const auto prior =
          std::count(capture->original_prisoner_ids.begin(),
                     capture->original_prisoner_ids.end(), actor_id);
      if (fresh + prior != 1)
        fail("NamedMilitaryCustodyCaptureActorMismatch", actor_id);
      transferred = prior == 1;
    }
    std::optional<WorldMoment> applied_at;
    SourceRef application_source;
    if (parent->domain == "military" &&
        parent->kind == "field_battle_segment" && context.field_applications) {
      application_source = {parent->domain, parent->kind, parent->id};
      applied_at = take(read_field_segment_application_moment(
          context.common, context.military, *context.field,
          {parent->domain, parent->kind, parent->id},
          *context.field_applications));
    }
    // 真精确成军来源不能只靠同 coarse tick 混过先后；旧成军仍保持其原 schema。
    if (applied_at && exact_formation_at &&
        take(compare_world_moments(*exact_formation_at, *applied_at)) > 0)
      fail("NamedMilitaryCustodyArmyFormationRequired", captured_army->id);
    // 旧粗 Tick 来源没有实际应用 M 就保持缺源；不能自动嵌为 (tick,0)。
    return {true,
            {receipt, change, captured_army, std::move(formation_source),
             formation_schema, transferred, std::move(applied_at),
             std::move(application_source),
             static_cast<Integer>(change - receipt->actor_changes.data()), 1},
            {}};
  } catch (Failure &error) {
    return {false, {}, std::move(error.error)};
  }
}
Outcome<std::optional<NamedMilitaryCustodyView>>
read_current_named_military_custody(const NamedMilitaryCustodyContext &context,
                                    const StableId &actor_id) {
  try {
    selection(context);
    const auto &common = context.common;
    const auto &military = context.military;
    const auto *actor = common.actor(actor_id);
    if (!actor || actor_id.empty())
      fail("UnknownActor", actor_id);
    const MilitaryUnitRecord *unit = nullptr;
    StableId original_keeper;
    for (const auto &row : military.military_units)
      if (has(row.captured, actor_id)) {
        if (unit)
          fail("NamedMilitaryCustodyDuplicateRoster", actor_id);
        unit = &row;
        for (const auto &[person, keeper] : row.captors)
          if (person == actor_id) {
            if (!original_keeper.empty() || keeper.empty())
              fail("NamedMilitaryCustodyCaptorRequired", actor_id);
            original_keeper = keeper;
          }
        if (original_keeper.empty())
          fail("NamedMilitaryCustodyCaptorRequired", actor_id);
      }
    const ArmyRecord *army_keeper = nullptr;
    const GarrisonRecord *garrison_keeper = nullptr;
    for (const auto &row : military.armies)
      if (has(row.prisoner_ids, actor_id)) {
        if (army_keeper || std::count(row.prisoner_ids.begin(),
                                      row.prisoner_ids.end(), actor_id) != 1)
          fail("NamedMilitaryCustodyDuplicateKeeper", actor_id);
        army_keeper = &row;
      }
    for (const auto &row : military.garrisons)
      if (has(row.prisoner_ids, actor_id)) {
        if (garrison_keeper || army_keeper ||
            std::count(row.prisoner_ids.begin(), row.prisoner_ids.end(),
                       actor_id) != 1)
          fail("NamedMilitaryCustodyDuplicateKeeper", actor_id);
        garrison_keeper = &row;
      }
    bool has_closure = false;
    for (const auto &release : military.named_custody.closures)
      if (release.actor_id == actor_id)
        has_closure = true;
    if (has_closure) {
      if (!context.releases || !context.releases->bound_to(common, military))
        fail("NamedMilitaryCustodyReleaseOriginalReadRequired", actor_id);
      const auto *released =
          take(context.releases->read_current_named_custody_release(actor_id));
      if (released) {
        if (army_keeper || garrison_keeper)
          fail("NamedMilitaryCustodyReleaseOutcomeNotApplied", actor_id);
        // 只有完整原关闭来源证明才允许原 captured/captors 留历史而不再占用
        // prisoner_ids。
        return {true, std::nullopt, {}};
      }
    }
    bool field_captured = false;
    if (context.field)
      for (const auto &[battle, session] : context.field->sessions)
        for (const auto &[id, formation] : session.formations)
          if (has(formation.captured, actor_id))
            field_captured = true;
    // 场内已判俘而段提交尚未落原账时，不能借空原名单把人物当自由身。
    if (field_captured && (!unit || (!army_keeper && !garrison_keeper)))
      fail("NamedMilitaryCustodyFieldSettlementRequired", actor_id);
    const auto naval_keeper = context.naval
                                  ? naval_custodian(*context.naval, actor_id)
                                  : std::nullopt;
    if (!unit && !army_keeper && !garrison_keeper && !naval_keeper)
      return {true, std::nullopt, {}};
    if ((!army_keeper && !garrison_keeper) || !actor->alive || actor->available)
      fail("NamedMilitaryCustodyOutcomeNotApplied", actor_id);
    const MilitaryCasualtyFact *captured = nullptr;
    const MilitaryActorChange *change = nullptr;
    std::size_t captured_position = 0;
    for (const auto &receipt : military.casualty_facts)
      for (const auto &effect : receipt.actor_changes)
        if (effect.actor_id == actor_id && effect.status == "captured") {
          capture_parent(context, receipt);
          const auto order =
              position(common, {"military", "military_casualties", receipt.id});
          if (!captured || order > captured_position) {
            captured = &receipt;
            change = &effect;
            captured_position = order;
          } else if (order == captured_position) {
            fail("NamedMilitaryCustodyDuplicateCapture", actor_id);
          }
        }
    if (!captured || !change || change->captor_army_id.empty())
      fail("NamedMilitaryCustodyCaptureSourceRequired", actor_id);
    NamedMilitaryCustodyView out;
    out.actor_id = actor_id;
    if (unit)
      out.source_unit_id = unit->id;
    out.capture_source = {"military", "military_casualties", captured->id};
    out.current_custody_source = out.capture_source;
    out.capture_tick = captured->tick;
    out.location_binding = actor->location;
    StableId keeper = change->captor_army_id;
    const NavalCaptureFact *latest = nullptr;
    std::size_t latest_position = 0;
    const auto *capture_parent_head =
        common.state().facts.find(captured->battle_id);
    if (!capture_parent_head)
      fail("NamedMilitaryCustodyCaptureParentRequired", actor_id);
    const auto original_capture_position =
        position(common, {capture_parent_head->domain,
                          capture_parent_head->kind, capture_parent_head->id});
    if (context.naval)
      for (const auto &row : context.naval->battle.captures)
        if (has(row.prisoner_ids, actor_id) ||
            has(row.original_prisoner_ids, actor_id)) {
          const auto order =
              position(common, {"military_naval", "naval_capture", row.id});
          if (order < original_capture_position)
            continue;
          if (row.tick < captured->tick || row.tick > common.tick())
            fail("NamedMilitaryCustodyNavalTransferRequired", actor_id);
          if (!latest || order > latest_position) {
            latest = &row;
            latest_position = order;
          }
        }
    if (latest) {
      const auto *fleet = unique(context.naval->fleets, latest->to_fleet_id);
      if (!fleet || (!latest->casualty_source_id.empty() &&
                     latest->id == captured->battle_id &&
                     latest->casualty_source_id != captured->id))
        fail("NamedMilitaryCustodyNavalTransferRequired", actor_id);
      keeper = fleet->army_id;
      out.current_custody_source = {"military_naval", "naval_capture",
                                    latest->id};
    }
    std::size_t custody_position = std::max(captured_position, latest_position);
    // 驻军被接管是真正转手，不新造俘获收据或重开期限。每次转手必须接前一真实返营。
    std::vector<std::pair<std::size_t, const MilitaryOccupationFact *>>
        occupations;
    for (const auto &row : military.occupation_facts)
      if (row.custody_transfers) {
        bool relevant = false;
        for (const auto &transfer : *row.custody_transfers)
          if (transfer.person_id == actor_id) {
            if (relevant)
              fail("NamedMilitaryCustodyOccupationTransferRepeated", row.id);
            relevant = true;
          }
        if (!relevant)
          continue;
        const auto order =
            position(common, {"military", "military_occupation", row.id});
        if (order > custody_position)
          occupations.emplace_back(order, &row);
      }
    std::sort(occupations.begin(), occupations.end(),
              [](const auto &a, const auto &b) { return a.first < b.first; });
    for (const auto &[order, occupation] : occupations) {
      if (!context.access)
        fail("NamedMilitaryCustodyOccupationReadRequired", occupation->id);
      take(validate_applied_occupation_manifest(
          common, military, context.catalog, *context.access, occupation->id));
      const OccupationCustodyTransfer *transfer = nullptr;
      for (const auto &row : *occupation->custody_transfers)
        if (row.person_id == actor_id)
          transfer = &row;
      if (!transfer || transfer->from_army_id != keeper ||
          transfer->to_army_id != occupation->army_id ||
          transfer->to_army_id.empty() ||
          transfer->from_army_id == transfer->to_army_id)
        fail("NamedMilitaryCustodyOccupationTransferMismatch", occupation->id);
      const MilitaryReturnFact *returned = nullptr;
      for (const auto &row : military.return_facts)
        if (row.army_id == keeper &&
            row.garrison_id == occupation->garrison_id &&
            has(row.prisoner_ids, actor_id)) {
          const auto prior =
              position(common, {"military", "military_return", row.id});
          if (prior <= custody_position || prior >= order)
            continue;
          if (returned ||
              std::count(row.prisoner_ids.begin(), row.prisoner_ids.end(),
                         actor_id) != 1 ||
              row.tick < captured->tick || row.tick > occupation->tick)
            fail("NamedMilitaryCustodyOccupationReturnMismatch",
                 occupation->id);
          returned = &row;
        }
      if (!returned)
        fail("NamedMilitaryCustodyOccupationReturnRequired", occupation->id);
      keeper = transfer->to_army_id;
      custody_position = order;
      out.current_custody_source = {"military", "military_occupation",
                                    occupation->id};
    }
    if (unit && original_keeper != keeper)
      fail("NamedMilitaryCustodyCaptorSourceMismatch", actor_id);
    const auto *captor = unique(military.armies, keeper);
    if (!captor)
      fail("NamedMilitaryCustodyCaptorRequired", actor_id);
    out.captor_army_id = captor->id;
    if (army_keeper) {
      const auto *location = std::get_if<InArmyLocation>(&actor->location);
      if (army_keeper != captor || captor->status != "active" || !location ||
          location->army_id != captor->id)
        fail("NamedMilitaryCustodyLocationMismatch", actor_id);
      // BW07 原 collect_army_mouths 已读取同一
      // prisoner_ids；此处不扣第二遍口粮。
    } else {
      const auto *definition =
          unique(context.catalog.garrisons, garrison_keeper->id);
      const auto *location = std::get_if<SettlementLocation>(&actor->location);
      if (!definition || !location ||
          location->place_id != definition->place_id ||
          captor->status != "closed" ||
          captor->return_garrison_id.value_or(captor->garrison_id) !=
              garrison_keeper->id)
        fail("NamedMilitaryCustodyLocationMismatch", actor_id);
      const MilitaryReturnFact *returned = nullptr;
      std::size_t return_position = 0;
      for (const auto &row : military.return_facts)
        if (row.army_id == captor->id &&
            row.garrison_id == garrison_keeper->id &&
            has(row.prisoner_ids, actor_id)) {
          const auto order =
              position(common, {"military", "military_return", row.id});
          if (row.tick < captured->tick || row.tick > common.tick() ||
              order <= custody_position)
            fail("NamedMilitaryCustodyReturnSourceMismatch", actor_id);
          if (!returned || order > return_position) {
            returned = &row;
            return_position = order;
          }
        }
      if (!returned)
        fail("NamedMilitaryCustodyOriginalReturnRequired", actor_id);
      out.garrison_id = garrison_keeper->id;
      out.current_custody_source = {"military", "military_return",
                                    returned->id};
      // BW07 原营地嘴数同样读取 prisoner_ids；本投影不保余额或另建供给规则。
    }
    return {true, std::move(out), {}};
  } catch (Failure &error) {
    return {false, {}, std::move(error.error)};
  }
}
Status validate_current_named_military_custody(
    const NamedMilitaryCustodyContext &context) {
  try {
    selection(context);
    std::set<StableId> actors;
    for (const auto &row : context.military.military_units)
      for (const auto &actor : row.captured)
        if (context.common.actor(actor))
          actors.insert(actor);
    for (const auto &row : context.military.armies)
      for (const auto &actor : row.prisoner_ids)
        if (context.common.actor(actor))
          actors.insert(actor);
    for (const auto &row : context.military.garrisons)
      for (const auto &actor : row.prisoner_ids)
        if (context.common.actor(actor))
          actors.insert(actor);
    if (context.field)
      for (const auto &[battle, session] : context.field->sessions)
        for (const auto &[id, formation] : session.formations)
          for (const auto &actor : formation.captured)
            if (context.common.actor(actor))
              actors.insert(actor);
    for (const auto &actor : actors)
      take(read_current_named_military_custody(context, actor));
    return common_success();
  } catch (Failure &error) {
    return {false, {}, std::move(error.error)};
  }
}
Outcome<std::optional<NamedMilitaryCustodyView>>
read_named_military_custody_command_gate(
    const NamedMilitaryCustodyContext &context, const StableId &actor_id,
    NamedMilitaryCustodyCommandKind kind) {
  auto result = read_current_named_military_custody(context, actor_id);
  if (!result.ok || !result.value)
    return result;
  // 名单穷举；新增中央命令不能因默认分支或可伪造标签自动获得许可。
  switch (kind) {
  case NamedMilitaryCustodyCommandKind::half_day_wait:
    return result;
  case NamedMilitaryCustodyCommandKind::co_located_dialogue: {
    // 本版战场内不开放对白；真实在战绑定不能借冻结的 InArmy 地址冒世界营地。
    if (context.field)
      for (const auto &[id, binding] : context.field->army_bindings) {
        bool held = binding.army_id == result.value->captor_army_id;
        for (const auto &member : binding.members)
          if (member.actor_id == actor_id)
            held = true;
        if (held)
          return {false, {}, common_failure("ActorInCustody", actor_id).error};
      }
    const auto *actor = context.common.actor(actor_id);
    bool placed =
        actor && std::holds_alternative<SettlementLocation>(actor->location);
    if (!placed && actor &&
        std::holds_alternative<InArmyLocation>(actor->location)) {
      for (const auto &army : context.military.armies)
        if (army.id == result.value->captor_army_id) {
          const auto *keeper = context.common.actor(army.commander_actor_id);
          placed = keeper &&
                   std::holds_alternative<SettlementLocation>(keeper->location);
        }
    }
    if (!placed)
      return {false, {}, common_failure("ActorInCustody", actor_id).error};
    // 这里只确认世界层位置类别，真实同处和具体地点仍由原互动空间校验负责。
    return result;
  }
  case NamedMilitaryCustodyCommandKind::other_world_change:
  default:
    return {false, {}, common_failure("ActorInCustody", actor_id).error};
  }
}
Status require_named_military_actor_not_in_custody(
    const NamedMilitaryCustodyContext &context, const StableId &actor_id) {
  auto result = read_named_military_custody_command_gate(
      context, actor_id, NamedMilitaryCustodyCommandKind::other_world_change);
  if (!result.ok)
    return {false, {}, std::move(result.error)};
  return common_success();
}
} // namespace hanmo::resident
