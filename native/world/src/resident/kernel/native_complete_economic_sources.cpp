#include "native_complete_economic_sources.hpp"
#include "growth_market_history_integration.hpp"
#include "native_political_world_scope.hpp"
#include "resident/domains/local_economy/economic_world_scope.hpp"
#include "world_bundle.hpp"
namespace hanmo::resident {
namespace {
// 所有深前序由原域 owner 自持；本层只保完整 owning 图与原 bindings。
class EconomicScope final : public NativeCompleteWorldSourceScope {
  const WorldBundle &world_;
  WorldMoment at_;
  std::unique_ptr<NativeEconomicWorldScope> original_;
  EconomicExactReadBinding read_;
  std::optional<EconomicExactWriteBinding> write_;
  NativeCompleteWorldSourceExports exports_;
  EconomicScope(const WorldBundle &w, const WorldMoment &at,
                std::unique_ptr<NativeEconomicWorldScope> original)
      : world_(w), at_(at), original_(std::move(original)),
        read_(original_->read().bindings()) {
    exports_.economic_binding = &read_;
    if (auto *writer = original_->write()) {
      write_.emplace(writer->bindings());
      exports_.economic_write_binding = &*write_;
    }
  }

public:
  static Outcome<std::shared_ptr<EconomicScope>>
  create(const WorldBundle &w, WorldBundle *candidate,
         const NativeCompleteWorldCoreSources &core,
         const NativeCompleteDomainPrerequisiteOwner &prerequisites,
         const WorldMoment &at, const DynamicStepInput *phase) {
    if (!w.catalog || &core.characters.early.world != &w ||
        !prerequisites.bound_to(w) || (candidate && candidate != &w))
      return {false, {}, common_failure("CompleteEconomicWorldMismatch").error};
    auto now = w.actual_phase_moment();
    if (!now.ok)
      return {false, {}, std::move(now.error)};
    auto same = compare_world_moments(now.value, at);
    if (!same.ok)
      return {false, {}, std::move(same.error)};
    if (same.value)
      return {
          false, {}, common_failure("CompleteEconomicMomentMismatch").error};
    const auto &installed = prerequisites.sources().installed_original;
    NativePoliticalWorldLateRead political_late;
    political_late.installation =
        installed ? &installed->installation() : nullptr;
    political_late.actual_phase = phase;
    auto political =
        NativePoliticalWorldScope::create_read(w, core, at, political_late);
    if (!political.ok)
      return {false, {}, std::move(political.error)};
    NativeEconomicPaymentWorldSources payment_sources;
    payment_sources.political = political.value;
    // 已选 P11/Work/Repair/Transport 的真前序缺失，由原有限构造逐项拒绝。
    auto payment_result =
        NativeEconomicPaymentWorldScope::create(w, std::move(payment_sources));
    if (!payment_result.ok)
      return {false, {}, std::move(payment_result.error)};
    auto payments = checkpoint_share(std::move(payment_result.value));
    NativeEconomicFifoWorldLateSources fifo_late;
    fifo_late.installed = installed;
    fifo_late.political = std::shared_ptr<const PoliticalExactReadLease>(
        political.value, &political.value->political_read_lease());
    if (const auto *takeover = political.value->takeover_context())
      fifo_late.takeover = std::shared_ptr<const PoliticalTakeoverContext>(
          political.value, takeover);
    if (candidate && candidate->production_cycles)
      fifo_late.original_payments =
          std::shared_ptr<const EconomicOriginalPaymentRead>(payments,
                                                             &payments->read());
    auto fifo_result = NativeEconomicFifoWorldScope::create(
        core, std::move(fifo_late), phase,
        candidate && candidate->production_cycles
            ? &*candidate->production_cycles
            : nullptr);
    if (!fifo_result.ok)
      return {false, {}, std::move(fifo_result.error)};
    auto fifo = checkpoint_share(std::move(fifo_result.value));
    const auto actual_fifo =
        std::shared_ptr<const NativeEconomicFifoRead>(fifo, &fifo->fifo());
    NativeHouseholdWorldLateSources household_late;
    household_late.transport_business =
        core.characters.early.transport_business;
    household_late.actual_market = actual_fifo;
    auto household_result = NativeHouseholdWorldScope::create(
        core, std::move(household_late), phase);
    if (!household_result.ok)
      return {false, {}, std::move(household_result.error)};
    auto household = checkpoint_share(std::move(household_result.value));
    NativeEconomicWorldLateSources late;
    if (candidate) {
      auto *market = candidate->services.market_history();
      if (!market)
        return {false,
                {},
                common_failure("CompleteEconomicMarketOwnerRequired").error};
      auto capture =
          NativeEconomicMarketCaptureServices::create(*market, actual_fifo);
      if (!capture.ok)
        return {false, {}, std::move(capture.error)};
      late.market = checkpoint_share(std::move(capture.value));
    }
    auto original = NativeEconomicWorldScope::create(
        w, political.value, std::move(payments), std::move(fifo),
        std::move(household), std::move(late), candidate);
    if (!original.ok)
      return {false, {}, std::move(original.error)};
    return {true,
            checkpoint_share(std::unique_ptr<EconomicScope>(
                new EconomicScope(w, at, std::move(original.value)))),
            {}};
  }
  bool bound_to(const WorldBundle &w) const noexcept override {
    return &w == &world_ && original_->bound_to(w);
  }
  NativeCompleteWorldRoute route() const noexcept override {
    return NativeCompleteWorldRoute::economic;
  }
  const WorldMoment &actual_at() const noexcept override { return at_; }
  const NativeCompleteWorldSourceExports &sources() const noexcept override {
    return exports_;
  }
};
} // namespace
Outcome<std::shared_ptr<const NativeCompleteWorldSourceScope>>
make_native_complete_economic_read_scope(
    const WorldBundle &w, const NativeCompleteWorldCoreSources &core,
    const NativeCompleteDomainPrerequisiteOwner &original,
    const WorldMoment &at) {
  auto scope = EconomicScope::create(w, nullptr, core, original, at, nullptr);
  if (!scope.ok)
    return {false, {}, std::move(scope.error)};
  return {true, std::move(scope.value), {}};
}
Outcome<std::shared_ptr<NativeCompleteWorldSourceScope>>
make_native_complete_economic_write_scope(
    WorldBundle &w, const NativeCompleteWorldCoreSources &core,
    NativeCompleteDomainPrerequisiteOwner &original,
    CompleteExactWorldBindingOwner &complete, const WorldMoment &at,
    ExactPhaseWriteContext *writes, const DynamicStepInput *phase) {
  if (&complete != &core.characters.early.complete || writes)
    return {
        false, {}, common_failure("CompleteEconomicWriteScopeMismatch").error};
  auto scope = EconomicScope::create(w, &w, core, original, at, phase);
  if (!scope.ok)
    return {false, {}, std::move(scope.error)};
  return {true, std::move(scope.value), {}};
}
} // namespace hanmo::resident
