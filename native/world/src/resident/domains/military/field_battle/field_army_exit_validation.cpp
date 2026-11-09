#include "field_army_exit_validation.hpp"
#include "field_army_exit_return.hpp"
#include "field_ability_sources.hpp"
#include "field_army_exit_ordinary_loss.hpp"
#include "field_author_data.hpp"
#include "field_exact_contact_source.hpp"
#include "field_group_handoff.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/storage/common_store.hpp"
#include <map>
#include <set>
namespace hanmo::resident {
using namespace movement_detail;
// 无新世界时钟、缓存flags或平行损失账。原表每条源与每个Common头双向覆盖。
Status validate_field_army_exit_sources(const FieldExactContactReadContext &x,
    const FieldBattleCatalog &catalog, const FieldBattleAbilityReadContext &a,
    const FieldGroupHandoffReadContext &handoffs) {
  return status([&] {
    if (!x.original_military_access || a.exact_contacts != &x ||
        &handoffs.common != &x.common || &handoffs.military != &x.military ||
        &handoffs.field != &x.field || &handoffs.groups != &x.groups ||
        &handoffs.movement != &x.movement || handoffs.exact_contacts != &x)
      fail("FieldArmyExitSameWorldSourcesRequired");
    take(validate_field_army_exit_choice_sources(x, catalog, a, *x.original_military_access));
    const auto now = take(x.clock.read_current_moment());
    std::map<StableId, const FieldArmyExitStep *> steps;
    std::map<StableId, const FieldArmyExitPlan *> ready;
    std::set<std::pair<StableId, StableId>> initialized;
    for (const auto &[id, plan] : x.field.army_exit_plans) {
      if (!plan.terminal_segment) {
        if (plan.ordinary_deadline || !plan.rally_formations.empty() || !plan.steps.empty() ||
            plan.ready_source || plan.ready_at || plan.ready_applied_at || plan.actual_continuation)
          fail("FieldArmyExitUnexpectedRallySource", id);
        continue;
      }
      if (!initialized.emplace(plan.battle_id, plan.army_id).second)
        fail("FieldArmyExitChoiceAmbiguous", plan.army_id);
      const auto state = take(read_field_army_exit_progress(x, catalog, a,
          {"military", "field_army_exit_choice", id}));
      std::size_t survivors = 0;
      for (const auto &f : state.formations)
        survivors += f.ordinary_member_ids.size() + f.origin->retained_officer_ids.size();
      for (const auto &step : plan.steps)
        if (!steps.emplace(step.fact_id, &step).second)
          fail("FieldArmyExitStepSourceAmbiguous", step.fact_id);
      if (plan.ready_source) {
        if (!ready.emplace(plan.ready_source->id, &plan).second)
          fail("FieldArmyExitReadySourceAmbiguous", plan.ready_source->id);
        take(read_field_army_exit_ready_source(x, catalog, a, *plan.ready_source));
        if (plan.actual_continuation)
          take(read_field_army_exit_continuation_source(x, catalog, a, *plan.actual_continuation));
        else if (!take(read_field_army_exit_return_wait(x, catalog, a, *plan.ready_source)))
          fail("FieldArmyExitReadyOriginalContinuationRequired", plan.ready_source->id);
      } else {
        if (plan.ready_at || plan.ready_applied_at || plan.actual_continuation)
          fail("FieldArmyExitReadySourceRequired", id);
        // 原发布边界必须已实走完该窗。0幸存保真实无返回/待原calendar，
        // 不写空ready、不销钱包，也不将全损未实现报告成军队已关闭。
        if (survivors && x.common.tick() == now.half_day_tick &&
            take(compare_fractions(now.sub_half_day, CanonicalFraction{})) == 0 &&
            (state.at.tick != now.half_day_tick || state.at.t != 0))
          fail("FieldArmyExitUnconsumedWindowRequired", id);
      }
    }
    // 真实终局需要退出的每一军必须已初始化，不能删整张计划绕过任何step检查。
    for (const auto &[battle, session] : x.field.sessions) {
      if (session.state != FieldBattleState::Ended || !session.result) continue;
      std::set<StableId> armies;
      for (const auto &[id, f] : session.formations) armies.insert(f.army_id);
      for (const auto &army : armies)
        if (take(read_field_army_terminal_exit_required(session, army)) &&
            !initialized.contains({battle, army}))
          fail("FieldArmyExitRallyOriginRequired", army);
    }
    for (const auto &[id, head] : x.common.state().facts.rows()) {
      if (head.kind == "field_army_exit_step" && (head.domain != "military" || !steps.contains(id)))
        fail("FieldArmyExitStepSourceRequired", id);
      if (head.kind == "field_army_exit_ready" && (head.domain != "military" || !ready.contains(id)))
        fail("FieldArmyExitReadySourceRequired", id);
    }
    // 反向防止新S1开程/取消引用删掉的ready，不能只遍历Field正文。
    for (const auto &[id, opening] : x.groups.openings.rows())
      if (opening.military_order && opening.military_order->original_return_cause) {
        const auto &source = *opening.military_order->original_return_cause;
        if (source.domain == "military" && source.kind == "field_army_exit_ready")
          take(read_field_army_exit_continuation_source(x, catalog, a,
              {"travel", "group_movement_opening", id}));
      }
    for (const auto &[id, control] : x.groups.controls.rows())
      if (control.authority_source.domain == "military" && control.authority_source.kind == "field_army_exit_ready") {
        const auto found = ready.find(control.authority_source.id);
        if (found == ready.end() || !found->second->actual_continuation)
          fail("FieldArmyExitReturnContinuationSourceRequired", id);
        const auto actual = take(read_field_army_exit_continuation_source(x, catalog, a, *found->second->actual_continuation));
        if (actual.cancellation != &control) fail("FieldArmyExitReturnOriginalCancellationRequired", id);
      }
    // 原ordinary完整roster、真实S4和S1后果批次由唯一低层owner反覆盖。
    take(validate_field_army_exit_ordinary_loss_applications(handoffs));
  });
}
} // namespace hanmo::resident
