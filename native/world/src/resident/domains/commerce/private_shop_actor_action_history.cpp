// PrivateShop 只枚举原 operation，实际 /2 起止头与 S0 真变化构成占用窗。
#include "private_shop_action_time.hpp"
#include "resident/domains/local_economy/economy_fixed_actor_history_detail.hpp"
namespace hanmo::resident {
Status visit_private_shop_actor_action_history(
    const PrivateShopOperationSourceContext &x, const ActionTimeRegistration &r,
    const StableId &actor, ActionTimeActorHistorySink &sink) {
  using namespace economy_actor_history_detail;
  try {
    registration(r, private_shop_operation_action_time_registration());
    if ((x.shops.state.version != 2 && x.shops.state.version != 3) ||
        x.sources.schema_version != 1 || x.capabilities.schema_version != 1)
      fail("PrivateShopActorHistoryOriginalSchemaRequired");
    const auto &common = x.shops.common;
    if (actor.empty() || !common.actor(actor))
      fail("EconomicActorHistoryActorRequired", actor);
    const auto now = take(x.clock.read_current_moment());
    if (&x.body.common != &common || compare(now, x.body.actual_now) ||
        (x.body.elapsed && x.body.elapsed != &x.elapsed))
      fail("PrivateShopActorHistoryContextMismatch", actor);
    for (const auto &entry : x.shops.state.operations.rows()) {
      const auto &o = entry.second;
      if (o.id != entry.first || o.actor_id.empty())
        fail("PrivateShopActorHistoryOriginalOperationRequired", entry.first);
      const auto &started = header(common, o.id, "private_shops",
                                   "private_shop_operating_started");
      if (take(read_private_shop_bootstrap_payload_schema(
              common, x.shops.state, x.sources, x.capabilities, started)) != 2)
        fail("PrivateShopActorHistoryExactAdoptionRequired", o.id);
      const PrivateShopOperationAcceptance *accepted = nullptr;
      for (const auto &a : x.sources.accepted_operations)
        if (a.operation_ref == o.id) {
          if (accepted)
            fail("PrivateShopActorHistoryDuplicateAcceptance", o.id);
          accepted = &a;
        }
      if (!accepted)
        fail("PrivateShopActorHistoryExactAdoptionRequired", o.id);
      take(sink.original_action_owner(o.id));
      if (o.actor_id != actor)
        continue;
      const PrivateShopOperationTerminalSource *terminal = nullptr;
      for (const auto &t : x.sources.terminal_sources)
        if (t.operation_ref == o.id) {
          if (terminal)
            fail("PrivateShopActorHistoryDuplicateTerminal", o.id);
          terminal = &t;
        }
      const PrivateShopDayResult *result = nullptr;
      const FactRecord *ended = nullptr;
      if (terminal) {
        result = x.shops.state.operating_results.find(
            terminal->original_result_ref.id);
        if (!result || result->id != o.result_fact_id ||
            result->operation_id != o.id || result->actor_id != actor ||
            result->shop_id != o.shop_id ||
            terminal->original_result_ref.domain != "private_shops" ||
            terminal->original_result_ref.kind !=
                (result->completed ? "private_shop_operating_completed"
                                   : "private_shop_operating_interrupted"))
          fail("PrivateShopActorHistoryTerminalIdentityMismatch", o.id);
        ended =
            &header(common, result->id, terminal->original_result_ref.domain,
                    terminal->original_result_ref.kind);
        if (take(read_private_shop_bootstrap_payload_schema(
                common, x.shops.state, x.sources, x.capabilities, *ended)) != 2)
          fail("PrivateShopActorHistoryExactTerminalRequired", o.id);
      } else if (!o.result_fact_id.empty())
        fail("PrivateShopActorHistoryExactTerminalRequired", o.id);
      FixedIntervals intervals(common, o.id, accepted->accepted_at, started,
                               now, sink);
      FixedChain chain(o.id, "private_shops", "private_shop_operating_started",
                       o.id, 2);
      for (const auto &t : x.elapsed.fixed_service_transitions)
        if (t.action_ref == o.id) {
          const bool first = chain.next(t);
          if (first) {
            const auto *cause = std::get_if<FixedOwnerAdopted>(&t.cause);
            if (t.kind != FixedTransitionKind::started ||
                t.transition_id != o.id || !cause ||
                cause->owner_source_schema != 2 ||
                !same_source_ref(cause->owner_source_ref,
                                 t.execution_source_ref) ||
                compare(t.occurred_at, accepted->accepted_at))
              fail("PrivateShopActorHistoryActualFixedStartRequired", o.id);
          } else if (t.kind == FixedTransitionKind::terminal) {
            const auto *cause = std::get_if<FixedOwnerTerminated>(&t.cause);
            if (!cause || !terminal || !result ||
                cause->owner_result_schema != 2 ||
                !same_source_ref(cause->owner_result_ref,
                                 terminal->original_result_ref) ||
                t.transition_id != result->id ||
                compare(t.occurred_at, terminal->occurred_at) ||
                t.terminal_disposition !=
                    std::optional<FixedTerminalDisposition>{
                        result->completed
                            ? FixedTerminalDisposition::completed
                            : (result->reason == "cancelled"
                                   ? FixedTerminalDisposition::cancelled
                                   : FixedTerminalDisposition::interrupted)})
              fail("PrivateShopActorHistoryActualFixedTerminalRequired", o.id);
          } else {
            condition_change(x.body, actor, o.id, t);
            condition_interval(intervals, t);
          }
        }
      if (terminal)
        intervals.end(terminal->occurred_at, ended);
      intervals.finish();
    }
    return common_success();
  } catch (const Failure &failure) {
    return {false, {}, failure.error};
  }
}
} // namespace hanmo::resident
