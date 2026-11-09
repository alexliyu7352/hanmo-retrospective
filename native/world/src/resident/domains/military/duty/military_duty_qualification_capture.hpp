// Internal synchronous capture owner. This is not a new persistent source.
#pragma once
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/storage/common_store.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
namespace hanmo::resident::duty_detail {
class MilitaryDutyQualificationCapture {
 const CommonStore& common_;
 CheckpointMemoryCharge memory_;
 CommonActualFactPrefix prefix_;
 Integer observed_current_count_=0;
 static CheckpointMemoryCharge reserve(const CommonStore& common,const CurrentCommonFactPrefix& value){
  movement_detail::take(read_actual_common_fact_prefix(common,value));


  return CheckpointMemoryCharge(0);
 }
public:
 explicit MilitaryDutyQualificationCapture(const CommonStore& common)
  :MilitaryDutyQualificationCapture(common,movement_detail::take(read_current_common_fact_prefix(common))){}
 MilitaryDutyQualificationCapture(const CommonStore& common,const SourceRefBuildView& before)
  :MilitaryDutyQualificationCapture(common,movement_detail::take(read_actual_common_fact_prefix_before(common,before))){}
 MilitaryDutyQualificationCapture(const CommonStore& common,const CurrentCommonFactPrefix& value)
  :common_(common),memory_(reserve(common,value)),prefix_{value.common_fact_count,{}},
   observed_current_count_(movement_detail::take(read_current_common_fact_prefix(common)).common_fact_count){
  if(const auto* head=value.last_common_fact)prefix_.last_common_fact.emplace(SourceRef{head->domain,head->kind,head->id});
 }
 MilitaryDutyQualificationCapture(const MilitaryDutyQualificationCapture&)=delete;
 MilitaryDutyQualificationCapture& operator=(const MilitaryDutyQualificationCapture&)=delete;
 MilitaryDutyQualificationCapture(MilitaryDutyQualificationCapture&&)=delete;
 MilitaryDutyQualificationCapture& operator=(MilitaryDutyQualificationCapture&&)=delete;
 const CommonActualFactPrefix& prefix()const noexcept{return prefix_;}
 void validate_unchanged()const{
  movement_detail::take(read_actual_common_fact_prefix(common_,prefix_));
  if(movement_detail::take(read_current_common_fact_prefix(common_)).common_fact_count!=observed_current_count_)
   movement_detail::fail("MilitaryDutyQualificationCaptureChanged");
 }
 // The owning anchor survives callbacks; this is not an append/rollback epoch.
 // Every original source callback must still be read-only, must not retain the
 // capture, and must prove its own non-Common witness association and coverage.
};
}
