// 全具名原组成未拆转分支的实际SafeExit原读者；完整其余scope仍由专业owner接续。
#pragma once
#include "safe_exit_named_entry.hpp"
#include "safe_exit_progress.hpp"
namespace hanmo::resident {
struct SafeExitNamedOriginalContext {
 const SafeExitNamedEntryContext& entries;const SafeExitDecisionSourceContext& decisions;
 const TreatyTerminationContext& treaties;const SafeExitState& state;
 const OrganizationAuthorityClock& clock;
};
// 只接真实同carrier/composition、无货/军务/人口份额、未拆转/出生/死亡的原具名line。
// 原Decision历史权限仍须独立完整核；此类不定义费率/许可/派兵或推定失去权利后的处置。
class NativeNamedSafeExitOriginalRead final:public SafeExitEntryOriginalRead {
 const SafeExitNamedOriginalContext& x_;
 SafeExitEntryContext entry_context()const;
public:
 explicit NativeNamedSafeExitOriginalRead(const SafeExitNamedOriginalContext& x):x_(x){}
 Outcome<std::optional<SafeExitEntryTerm>> read_actual_safe_exit_entry_term(const BoundaryCrossingReceipt&)const override;
 Outcome<SafeExitEntryPartition> read_original_safe_exit_entry_partition(const BoundaryCrossingReceipt&,
  const TransportEntryManifestSource&,const TreatyPassageTerm&)const override;
 Status validate_original_safe_exit_entry_partition(const BoundaryCrossingReceipt&,
  const TransportEntryManifestSource&,const TreatyPassageTerm&,const SafeExitEntryPartition&)const override;
 Status validate_safe_exit_retained_progress(const SafeExitBinding&,const OrderedTable<SafeExitLine>&)const override;
 Status validate_actual_safe_exit_exit_adoption(const SafeExitBinding&,const SafeExitPassageContext&,
  const BoundaryCrossingReceipt&,const TransportEntryManifestSource&)const override;
};
}
