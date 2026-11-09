// P10 delegates precise Growth history to the unique Growth original reader.
#pragma once
#include "dynamic_result_time2.hpp"
#include "exact_time_profile.hpp"
namespace hanmo::resident {
struct WorldBundle;
class NativeGrowthResultOwnerRead final:public ExactResultOwnerRead {
 const WorldBundle&world_;const CompleteExactTimeRuntime&runtime_;
public:
 NativeGrowthResultOwnerRead(const WorldBundle&w,const CompleteExactTimeRuntime&r):world_(w),runtime_(r){}
 Outcome<ExactOwnerFactRead> read_selected_fact(const FactRecord&)const override;
 Outcome<WorldMoment> read_selected_source_moment(const MomentOwnerSource&)const override;
};
Outcome<std::shared_ptr<const ExactResultOwnerRead>> make_native_growth_result_sources(const WorldBundle&,const CompleteExactTimeRuntime&);
}
