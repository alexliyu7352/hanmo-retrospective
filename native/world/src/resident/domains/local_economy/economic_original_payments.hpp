// Selected original non-Calendar obligations. No new debt or balance is stored
// here.
#pragma once
#include "economic_calendar.hpp"
#include "economic_transport_sources.hpp"
#include "governance_work/governance_work_expense_contract.hpp"
namespace hanmo::resident {
struct EconomicObligationReadContext;
struct EconomicOriginalPaymentSourceIds {
  std::vector<StableId> obligation_ids, arrear_ids, payment_ids;
  const EconomicObligationReadContext *source_only = nullptr;
};
struct MaterialAidReadContext;
struct RepairServiceReadContext;
class EconomicOriginalPaymentRead {
public:
  virtual ~EconomicOriginalPaymentRead() = default;
  virtual Outcome<EconomicOriginalPaymentSourceIds>
  read_original_payment_source_ids() const = 0;
};
struct TransportOriginalPaymentReadContext {
  const EconomicTransportExpenseState &state;
  const EconomicTransportPaymentContext &sources;
};
// The compiled selected-module lease supplies every enabled owner; nullptr
// means the module is not selected. Unrecognized obligations remain rejected by
// Common S5.
class NativeEconomicOriginalPaymentRead final
    : public EconomicOriginalPaymentRead {
  const CommonStore &common_;
  const LocalEconomyState &economy_;
  const TransportOriginalPaymentReadContext *transport_;
  const GovernanceWorkExpenseReadContext *governance_;
  const MaterialAidReadContext *material_aid_;
  const RepairServiceReadContext *repair_service_;
  // 真P11可独立于4条付款路线选中，不以空路线掩盖原责任来源。
  const EconomicObligationReadContext *source_only_;

public:
  NativeEconomicOriginalPaymentRead(
      const CommonStore &, const LocalEconomyState &,
      const TransportOriginalPaymentReadContext *,
      const GovernanceWorkExpenseReadContext *,
      const MaterialAidReadContext * = nullptr,
      const RepairServiceReadContext * = nullptr,
      const EconomicObligationReadContext * = nullptr);
  Outcome<EconomicOriginalPaymentSourceIds>
  read_original_payment_source_ids() const override;
};
struct BornPopulationSourceContext;
struct EconomicExactValidationContext {
  const EconomicCalendarReadContext &calendar;
  const EconomicOriginalPaymentRead &payments;
  const EconomicWorkValidationContext &production;
  Integer selected_local_economy = 0;
  const BornPopulationSourceContext *born_population = nullptr;
};
} // namespace hanmo::resident
