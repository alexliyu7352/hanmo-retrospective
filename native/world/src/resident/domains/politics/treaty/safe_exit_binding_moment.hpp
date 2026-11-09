// 原binding的真实发生时点只由首次已采用正片末端证明，不从entry/付款时点补值。
#pragma once
#include "safe_exit_entry.hpp"
#include "resident/kernel/dynamic_result_time2.hpp"
namespace hanmo::resident {
// 此源时间leg不反入条款Knowledge/Authority；全世界另跑完整S6权限/lineage门。
Outcome<WorldMoment> read_safe_exit_binding_source_moment(
 const SafeExitEntryContext&,const MomentOwnerSource&);
bool native_safe_exit_binding_source_kind(const FactRecord&);
// 仅客观supporting source；没有真实观察来源时不生成Report或自动知情。
class NativeSafeExitBindingResultRead final:public ExactResultOwnerRead {
 const SafeExitEntryContext& context_;
public:explicit NativeSafeExitBindingResultRead(const SafeExitEntryContext& x):context_(x){}
 Outcome<ExactOwnerFactRead> read_selected_fact(const FactRecord&)const override;
 Outcome<WorldMoment> read_selected_source_moment(const MomentOwnerSource&)const override;
};
}
