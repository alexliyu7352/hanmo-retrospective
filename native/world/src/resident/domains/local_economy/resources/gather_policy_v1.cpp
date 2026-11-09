#include "gather_policy_v1.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
namespace hanmo::resident {
Outcome<GatherMethodPolicy> read_gather_method_policy_v1(GatherMethod method) {
 switch(method){
 case GatherMethod::gentle:
  return {true,{method,1,2,1,1,0,GatherFatigueClass::light,false,0,4},{}};
 case GatherMethod::standard:
  return {true,{method,1,1,5,4,1,GatherFatigueClass::normal,false,0,8},{}};
 case GatherMethod::intensive:
  return {true,{method,3,2,2,1,2,GatherFatigueClass::heavy,true,90,14},{}};
 }
 return {false,{},common_failure("UnsupportedGatherMethod").error};
}
Outcome<GatherPolicyEvaluation> evaluate_gather_policy_v1(
 GatherResourceKind kind,GatherMethod method,bool learned,bool has_tool,
 const CanonicalFraction& workers) {
 auto policy=read_gather_method_policy_v1(method);
 if(!policy.ok)return {false,{},std::move(policy.error)};
 if(!has_tool && method!=GatherMethod::gentle)
  return {false,{},common_failure("GatherRequiredToolMissing").error};
 auto sign=compare_fractions(workers,CanonicalFraction{});
 if(!sign.ok)return {false,{},std::move(sign.error)};
 if(sign.value<0)return {false,{},common_failure("NegativeGatherWork").error};
 Integer base=0;
 switch(kind){
 case GatherResourceKind::firewood:base=6;break;
 case GatherResourceKind::medicinal:base=learned?2:1;break;
 case GatherResourceKind::fish_and_game:base=3;break;
 default:return {false,{},common_failure("UnsupportedGatherResourceKind").error};
 }
 // All table values are bounded; the exact original fraction operations check
 // arithmetic budgets rather than multiplying unbounded worker integers here.
 const auto& p=policy.value;
 auto factor=normalize_fraction(std::to_string(base*p.harvest_numerator),
                               std::to_string(p.harvest_denominator*(has_tool?1:2)));
 if(!factor.ok)return {false,{},std::move(factor.error)};
 auto potential=multiply_fractions(workers,factor.value);
 if(!potential.ok)return {false,{},std::move(potential.error)};
 return {true,{std::move(potential.value),p,!has_tool},{}};
}
Outcome<GatherRealizedQuantity> realize_gather_quantity_v1(
 GatherMethod method,std::span<const CanonicalFraction> periods,
 Integer previously_harvested,Integer requested,Integer standing,Integer reserved,Integer output_capacity) {
 auto policy=read_gather_method_policy_v1(method);
 if(!policy.ok)return {false,{},std::move(policy.error)};
 if(previously_harvested<0||requested<0||standing<0||reserved<0||output_capacity<0)
  return {false,{},common_failure("NegativeGatherRealizationBound").error};
 CanonicalFraction sum;
 for(const auto& entitlement:periods){
  auto sign=compare_fractions(entitlement,CanonicalFraction{});
  if(!sign.ok)return {false,{},std::move(sign.error)};
  if(sign.value<0)return {false,{},common_failure("NegativeGatherEntitlement").error};
  auto next=add_fractions(sum,entitlement);
  if(!next.ok)return {false,{},std::move(next.error)};
  sum=std::move(next.value);
 }
 auto prior=normalize_fraction(std::to_string(previously_harvested),"1");
 if(!prior.ok)return {false,{},std::move(prior.error)};
 auto unpaid=subtract_fractions(sum,prior.value);
 if(!unpaid.ok)return {false,{},std::move(unpaid.error)};
 // floor(cumulative)-integer_paid == floor(cumulative-integer_paid).
 // This is recalculated from original periods and receipts, not saved carry.
 sum=std::move(unpaid.value);
 const auto& p=policy.value;
 // ceil(h*n/d)<=source iff h<=floor(source*d/n). Cap the harvest,
 // not merely the depletion, so shortage can never mint unsupported goods.
 auto source=normalize_fraction(std::to_string(std::min(standing,reserved)),"1");
 auto inverse=normalize_fraction(std::to_string(p.depletion_denominator),
                                 std::to_string(p.depletion_numerator));
 if(!source.ok)return {false,{},std::move(source.error)};
 if(!inverse.ok)return {false,{},std::move(inverse.error)};
 auto source_harvest=multiply_fractions(source.value,inverse.value);
 if(!source_harvest.ok)return {false,{},std::move(source_harvest.error)};
 auto source_cap=floor_fraction(source_harvest.value);
 if(!source_cap.ok)return {false,{},std::move(source_cap.error)};
 const Integer cap=std::min({requested,output_capacity,source_cap.value});
 auto cap_exact=normalize_fraction(std::to_string(cap),"1");
 if(!cap_exact.ok)return {false,{},std::move(cap_exact.error)};
 auto comparison=compare_fractions(sum,cap_exact.value);
 if(!comparison.ok)return {false,{},std::move(comparison.error)};
 auto harvest=floor_fraction(comparison.value>0?cap_exact.value:sum);
 if(!harvest.ok)return {false,{},std::move(harvest.error)};
 auto whole=normalize_fraction(std::to_string(harvest.value),"1");
 auto ratio=normalize_fraction(std::to_string(p.depletion_numerator),
                              std::to_string(p.depletion_denominator));
 if(!whole.ok)return {false,{},std::move(whole.error)};
 if(!ratio.ok)return {false,{},std::move(ratio.error)};
 auto debit=multiply_fractions(whole.value,ratio.value);
 if(!debit.ok)return {false,{},std::move(debit.error)};
 auto down=floor_fraction(debit.value);
 if(!down.ok)return {false,{},std::move(down.error)};
 auto down_exact=normalize_fraction(std::to_string(down.value),"1");
 if(!down_exact.ok)return {false,{},std::move(down_exact.error)};
 auto fraction=compare_fractions(debit.value,down_exact.value);
 if(!fraction.ok)return {false,{},std::move(fraction.error)};
 Integer depletion=down.value;
 if(fraction.value>0&&__builtin_add_overflow(depletion,Integer{1},&depletion))
  return {false,{},common_failure("GatherDepletionOverflow").error};
 if(depletion>standing||depletion>reserved||depletion<harvest.value)
  return {false,{},common_failure("GatherRealizationConservationFailure").error};
 // Zero is a genuine zero-output result. The owner must still retain actual
 // work, wear/fees/condition and the reason; never materialize a zero lot.
 return {true,{harvest.value,depletion,depletion-harvest.value},{}};
}
Outcome<GatherRecoveryRateView> read_gather_recovery_rate_v1(
 const CanonicalFraction& original,const WorldMoment& at,
 std::span<const GatherIntensiveRecoveryWindow> receipts) {
 auto duration=normalize_fraction("180","1"); // 90 days, world half-day units.
 if(!duration.ok)return {false,{},std::move(duration.error)};
 auto time=encode_world_moment(at);
 if(!time.ok)return {false,{},std::move(time.error)};
 GatherRecoveryRateView out;out.effective_rate=original;
 for(std::size_t i=0;i<receipts.size();++i){
  const auto& row=receipts[i];
  if(!valid_source_ref(row.gather_receipt_ref))
   return {false,{},common_failure("GatherRecoveryReceiptRequired").error};
  for(std::size_t j=0;j<i;++j)
   if(same_source_ref(row.gather_receipt_ref,receipts[j].gather_receipt_ref))
    return {false,{},common_failure("DuplicateGatherRecoveryReceipt",row.gather_receipt_ref.id).error};
  auto original_time=encode_world_moment(row.period_ended_at);
  if(!original_time.ok)return {false,{},std::move(original_time.error)};
  auto end=add_world_duration(row.period_ended_at,{duration.value});
  if(!end.ok)return {false,{},std::move(end.error)};
  auto started=compare_world_moments(row.period_ended_at,at);
  auto ended=compare_world_moments(at,end.value);
  if(!started.ok)return {false,{},std::move(started.error)};
  if(!ended.ok)return {false,{},std::move(ended.error)};
  if(started.value<=0&&ended.value<0){
   bool later=!out.active_until;
   if(out.active_until){auto cmp=compare_world_moments(*out.active_until,end.value);
    if(!cmp.ok)return {false,{},std::move(cmp.error)};later=cmp.value<0;}
   if(later)out.active_until=std::move(end.value);
  }
 }
 if(out.active_until){
  auto half=normalize_fraction("1","2");
  if(!half.ok)return {false,{},std::move(half.error)};
  auto rate=multiply_fractions(original,half.value);
  if(!rate.ok)return {false,{},std::move(rate.error)};
  out.effective_rate=std::move(rate.value);
 }
 return {true,std::move(out),{}};
}
}
