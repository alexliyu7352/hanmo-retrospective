#include "social_window_rules.hpp"
#include "resident/storage/common_store.hpp"
#include <limits>
namespace hanmo::resident {
Outcome<OriginalGiftBenefit> original_social_gift_benefit(
 OriginalAcceptedGiftClass kind,Integer prior,Integer base_trust){
 if((kind!=OriginalAcceptedGiftClass::ordinary_agreeable&&
     kind!=OriginalAcceptedGiftClass::precious_agreeable)||
    prior<0||prior==std::numeric_limits<Integer>::max()||
    base_trust<0||base_trust>1){
  auto error=common_failure("InvalidOriginalGiftBenefitInput",{},ErrorKind::internal);
  return {false,{},error.error};
 }
 const Integer factor=prior==0?10000:prior==1?5000:prior==2?2500:0;
 const Integer affinity=kind==OriginalAcceptedGiftClass::ordinary_agreeable?6:10;
 return {true,{prior+1,factor,affinity,affinity*factor/10000,
               base_trust*factor/10000},{}};
}
Outcome<bool> original_social_gift_window_contains(Integer accepted,Integer now){
 if(accepted<0||now<0||accepted>now){
  auto error=common_failure("InvalidOriginalGiftCalendarWindow",{},ErrorKind::internal);
  return {false,false,error.error};
 }
 // Both operands are nonnegative and accepted <= now, so this subtraction
 // cannot overflow even at the largest original Calendar tick.
 return {true,now-accepted<60,{}};
}
}
