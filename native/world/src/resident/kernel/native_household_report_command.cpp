// 预览/执行共用 S3 原 preflight，不复制报告知情、接触或受托规则。
#include "native_household_report_command.hpp"
#include "exact_complete_world.hpp"
#include "native_household_purchase_report_world.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/domains/local_economy/household_foundation_lease.hpp"
#include "world_bundle.hpp"
#include <utility>
namespace hanmo::resident {
namespace {
CheckpointMemoryCharge
quote_memory(const StableId &actor,
             const TellHouseholdPurchaseMarketReport &command) {


  return CheckpointMemoryCharge(0);
}
bool same_identity(const WorldIdentity &a, const WorldIdentity &b) {
  return a.world_id == b.world_id && a.execution_epoch == b.execution_epoch &&
         a.revision == b.revision && a.tick == b.tick;
}
bool same_command(const TellHouseholdPurchaseMarketReport &a,
                  const TellHouseholdPurchaseMarketReport &b) {
  return same_source_ref(a.purchase_plan_ref, b.purchase_plan_ref) &&
         a.report_id == b.report_id;
}
struct ActualReportCommand {
  // 逆序析构，report内引用绝不比其原Complete/phase活得更久。
  std::unique_ptr<PhaseCurrentReadScope> phase;
  std::unique_ptr<CompleteExactWorldBindingOwner> complete;
  std::unique_ptr<NativeHouseholdPurchaseReportWorldRead> report;
  const HouseholdPurchaseReportContext *context = nullptr;
};
Outcome<ActualReportCommand>
acquire(const WorldBundle &world, WorldBundle *write,
        const NativeHouseholdPurchaseReportWorldDependencies &dependencies) {
  if ((write && write != &world) || !world.catalog ||
      !world.catalog->exact_time || !world.read_stamps ||
      !world.world_timeline ||
      !exact_complete_selected(*world.catalog->exact_time))
    return {false,
            {},
            common_failure("HouseholdReportCommandCompleteRequired").error};
  auto now = world.actual_phase_moment();
  if (!now.ok)
    return {false, {}, now.error};
  ActualReportCommand out;
  if (!world.has_active_read_phase()) {


    auto phase_text_memory = CheckpointMemoryCharge(0);
    const StableId phase_name{"household_report_command"};
    const StableId provider_name{"interaction"};
    auto phase = world.enter_phase_read_context(
        phase_name, provider_name, {now.value, now.value}, now.value);
    if (!phase.ok)
      return {false, {}, phase.error};
    out.phase = std::move(phase.value);
  }
  auto complete = write ? write->complete_exact_time_bindings()
                        : world.complete_exact_time_bindings();
  if (!complete.ok)
    return {false, {}, complete.error};
  if (!complete.value || !complete.value->reads().original_core ||
      !complete.value->reads().household_foundation)
    return {
        false,
        {},
        common_failure("HouseholdReportCommandOriginalOwnerRequired").error};
  out.complete = std::move(complete.value);
  const auto &read = out.complete->reads();
  auto foundation = read.household_foundation->validate_bindings();
  if (!foundation.ok)
    return {false, {}, foundation.error};
  // 直接用同owner借出的原core；这里不回任何 public/Complete 工厂。
  auto report = NativeHouseholdPurchaseReportWorldRead::create(
      world, *read.original_core, *read.household_foundation, dependencies);
  if (!report.ok)
    return {false, {}, report.error};
  if (!report.value ||
      !report.value->bound_to(world, *read.household_foundation))
    return {false,
            {},
            common_failure("HouseholdReportCommandCandidateMismatch").error};
  out.report = std::move(report.value);
  auto context = out.report->context();
  if (!context.ok)
    return {false, {}, context.error};
  if (!context.value || &context.value->common != &world.common ||
      &context.value->interaction != &world.interaction ||
      &context.value->purchase !=
          &read.household_foundation->service().household())
    return {false,
            {},
            common_failure("HouseholdReportCommandContextMismatch").error};
  auto actual = context.value->disclosure.clock.read_current_moment();
  if (!actual.ok)
    return {false, {}, actual.error};
  auto same = compare_world_moments(actual.value, now.value);
  if (!same.ok)
    return {false, {}, same.error};
  if (same.value != 0)
    return {false,
            {},
            common_failure("HouseholdReportCommandMomentMismatch").error};
  out.context = context.value;
  return {true, std::move(out), {}};
}
} // namespace
NativeHouseholdReportCommandQuote::NativeHouseholdReportCommandQuote(
    const WorldIdentity &id, const StableId &actor,
    const TellHouseholdPurchaseMarketReport &input, const WorldMoment &at)
    : copied_fields(quote_memory(actor, input)), identity(id), actor_id(actor),
      command(input), actual_at(at) {}
NativeHouseholdReportCommandQuote::NativeHouseholdReportCommandQuote(
    NativeHouseholdReportCommandQuote &&other)
    : NativeHouseholdReportCommandQuote(
          static_cast<const NativeHouseholdReportCommandQuote &>(other)) {}
void NativeHouseholdReportCommandQuote::swap(
    NativeHouseholdReportCommandQuote &other) noexcept {
  copied_fields.swap(other.copied_fields);
  std::swap(identity, other.identity);
  actor_id.swap(other.actor_id);
  command.purchase_plan_ref.domain.swap(other.command.purchase_plan_ref.domain);
  command.purchase_plan_ref.kind.swap(other.command.purchase_plan_ref.kind);
  command.purchase_plan_ref.id.swap(other.command.purchase_plan_ref.id);
  command.report_id.swap(other.command.report_id);
  std::swap(actual_at.half_day_tick, other.actual_at.half_day_tick);
  actual_at.sub_half_day.swap(other.actual_at.sub_half_day);
}
NativeHouseholdReportCommandQuote &NativeHouseholdReportCommandQuote::operator=(
    NativeHouseholdReportCommandQuote other) {
  swap(other);
  return *this;
}
Outcome<NativeHouseholdReportCommandQuote>
preview_native_household_report_command(
    const WorldBundle &world, const StableId &actor,
    const TellHouseholdPurchaseMarketReport &command,
    const NativeHouseholdPurchaseReportWorldDependencies &dependencies) {
  auto actual = acquire(world, nullptr, dependencies);
  if (!actual.ok)
    return {false, {}, actual.error};
  const auto &x = *actual.value.context;
  auto valid =
      validate_tell_household_purchase_market_report_input(x, actor, command);
  if (!valid.ok)
    return {false, {}, valid.error};
  auto now = x.disclosure.clock.read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  return {true,
          NativeHouseholdReportCommandQuote{world.common.state().identity,
                                            actor, command, now.value},
          {}};
}
Outcome<const FactRecord *> apply_native_household_report_command_in_candidate(
    WorldBundle &world, const StableId &actor,
    const TellHouseholdPurchaseMarketReport &command,
    const NativeHouseholdReportCommandQuote &quote,
    const NativeHouseholdPurchaseReportWorldDependencies &dependencies) {
  if (quote.actor_id != actor ||
      !same_identity(quote.identity, world.common.state().identity) ||
      !same_command(quote.command, command))
    return {false, nullptr,
            common_failure("StaleHouseholdReportCommandQuote").error};
  auto actual = acquire(world, &world, dependencies);
  if (!actual.ok)
    return {false, nullptr, actual.error};
  const auto &x = *actual.value.context;
  auto now = x.disclosure.clock.read_current_moment();
  if (!now.ok)
    return {false, nullptr, now.error};
  auto same = compare_world_moments(quote.actual_at, now.value);
  if (!same.ok)
    return {false, nullptr, same.error};
  if (same.value != 0)
    return {false, nullptr,
            common_failure("StaleHouseholdReportCommandQuote").error};
  auto valid =
      validate_tell_household_purchase_market_report_input(x, actor, command);
  if (!valid.ok)
    return {false, nullptr, valid.error};
  auto written = tell_household_purchase_market_report_fact_in_candidate(
      world.common, world.interaction, x, actor, command);
  if (!written.ok)
    return {false, nullptr, written.error};
  const auto *header = written.value;
  if (!header || world.common.state().facts.find(header->id) != header ||
      header->domain != "interaction" ||
      header->kind != household_purchase_report_source_kind)
    return {false, nullptr,
            common_failure("HouseholdReportCommandActualResultRequired").error};
  return {true, header, {}};
}
} // namespace hanmo::resident
