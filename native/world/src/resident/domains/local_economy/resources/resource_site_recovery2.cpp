#include "resource_site_recovery2.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]] void fail(const char*code,const StableId&id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> x){if(!x.ok)throw Failure{x.error};return std::move(x.value);}
void take(Status x){if(!x.ok)throw Failure{x.error};}
CanonicalFraction integer(Integer n){return take(normalize_fraction(std::to_string(n),"1"));}
bool same(const ActorFoundationRuleRef&a,const ActorFoundationRuleRef&b){return a.id==b.id&&a.version==b.version&&a.catalog_digest==b.catalog_digest;}
}
Outcome<ResourceRecoveryReceipt2> preview_resource_recovery2(
 const ResourceSiteRecoveryState2&site,Integer capacity,const ResourceRecoveryRule&rule,
 const DynamicStepInput&step,const ResourceRecoveryDisposition&disposition,
 std::span<const GatherIntensiveRecoveryWindow> receipts){try{
 take(validate_resource_recovery_rule(rule));
 if(site.site_id.empty()||capacity<=0||site.remaining_units<0||site.remaining_units>capacity||
    site.last_recovery_tick<0||site.revision<0||!same(site.recovery_rule_ref,rule.rule_ref)||
    take(compare_fractions(site.fractional_remainder,CanonicalFraction{}))<0||
    take(compare_fractions(site.fractional_remainder,integer(1)))>=0||
    (site.remaining_units==capacity&&take(compare_fractions(site.fractional_remainder,CanonicalFraction{}))))
  fail("InvalidResourceRecoveryState2",site.site_id);
 if(step.from_tick!=site.last_recovery_tick||step.from_tick<0||
    step.from_tick==std::numeric_limits<Integer>::max()||step.to_tick!=step.from_tick+1)
  fail("ResourceRecoveryStepMismatch",site.site_id);
 if(!disposition.enabled&&disposition.original_sources.empty())fail("ResourceRecoveryDisableSourceRequired",site.site_id);
 for(std::size_t i=0;i<disposition.original_sources.size();++i){const auto&r=disposition.original_sources[i];
  if(!valid_source_ref(r.source_ref)||r.source_schema<1)fail("InvalidResourceRecoveryStateSource",site.site_id);
  for(std::size_t j=0;j<i;++j)if(same_source_ref(r.source_ref,disposition.original_sources[j].source_ref))fail("DuplicateResourceRecoveryStateSource",r.source_ref.id);
 }
 ResourceRecoveryReceipt2 out;out.site_id=site.site_id;out.from_tick=step.from_tick;out.to_tick=step.to_tick;out.rule_ref=rule.rule_ref;
 out.standing_before=out.standing_after=site.remaining_units;
 out.remainder_before=out.remainder_after=site.fractional_remainder;
 out.state_source_refs.assign(disposition.original_sources.begin(),disposition.original_sources.end());
 Integer numerator=rule.base_numerator;
 for(const auto&season:rule.season_rows)if(season.from_tick<=step.from_tick&&step.from_tick<season.until_tick){numerator=season.numerator;out.season_id=season.season_id;break;}
 const WorldMoment opening{step.from_tick,{}};
 auto rate=take(read_gather_recovery_rate_v1(take(normalize_fraction(std::to_string(numerator),std::to_string(rule.denominator))),opening,receipts));
 out.effective_rate=rate.effective_rate;
 const HalfDayDuration ninety_days{integer(180)};
 for(const auto&r:receipts){const auto until=take(add_world_duration(r.period_ended_at,ninety_days));
  if(take(compare_world_moments(r.period_ended_at,opening))<=0&&take(compare_world_moments(opening,until))<0)out.intensive_receipt_refs.push_back(r.gather_receipt_ref);
 }
 if(site.remaining_units==capacity){out.remainder_after={};if(disposition.enabled)out.discarded_whole_units=take(floor_fraction(out.effective_rate));return {true,std::move(out),{}};}
 if(!disposition.enabled){if(rule.disabled_policy==ResourceDisabledRecoveryPolicy::reset_on_disabled)out.remainder_after={};return {true,std::move(out),{}};}
 if(numerator==0){if(take(compare_fractions(site.fractional_remainder,CanonicalFraction{})))fail("NonrenewableResourceRemainder",site.site_id);return {true,std::move(out),{}};}
 auto raw=take(add_fractions(site.fractional_remainder,out.effective_rate));
 const auto potential=take(floor_fraction(raw));out.recovered_units=std::min(potential,capacity-site.remaining_units);
 out.discarded_whole_units=potential-out.recovered_units;out.standing_after+=out.recovered_units;
 out.remainder_after=out.standing_after==capacity?CanonicalFraction{}:take(subtract_fractions(raw,integer(potential)));
 return {true,std::move(out),{}};
 }catch(const Failure&f){return {false,{},f.error};}}
Outcome<ResourceRecoveryReceipt2> apply_resource_recovery2_in_candidate(
 ResourceSiteRecoveryState2&site,Integer capacity,const ResourceRecoveryRule&rule,
 const DynamicStepInput&step,const ResourceRecoveryDisposition&disposition,
 std::span<const GatherIntensiveRecoveryWindow> receipts){
 auto out=preview_resource_recovery2(site,capacity,rule,step,disposition,receipts);
 if(!out.ok)return out;
 if(site.revision==std::numeric_limits<Integer>::max())return {false,{},common_failure("ResourceSiteRevisionOverflow",site.site_id).error};
 // Allocate exact state first; then the actual numerical state changes together.
 auto next=out.value.remainder_after;
 site.fractional_remainder.swap(next);site.remaining_units=out.value.standing_after;
 site.last_recovery_tick=step.to_tick;++site.revision;
 return out;
}
Status validate_resource_recovery_receipt2_against_original(
 const ResourceSiteRecoveryState2&before,Integer capacity,const ResourceRecoveryRule&rule,
 const DynamicStepInput&step,const ResourceRecoveryDisposition&disposition,
 std::span<const GatherIntensiveRecoveryWindow>originals,const ResourceRecoveryReceipt2&actual){try{
 const auto expected=take(preview_resource_recovery2(before,capacity,rule,step,disposition,originals));
 if(actual.site_id!=expected.site_id||actual.from_tick!=expected.from_tick||actual.to_tick!=expected.to_tick||
    !same(actual.rule_ref,expected.rule_ref)||actual.season_id!=expected.season_id||
    actual.standing_before!=expected.standing_before||actual.standing_after!=expected.standing_after||
    actual.recovered_units!=expected.recovered_units||actual.discarded_whole_units!=expected.discarded_whole_units||
    take(compare_fractions(actual.effective_rate,expected.effective_rate))||
    take(compare_fractions(actual.remainder_before,expected.remainder_before))||
    take(compare_fractions(actual.remainder_after,expected.remainder_after))||
    actual.intensive_receipt_refs.size()!=expected.intensive_receipt_refs.size()||
    actual.state_source_refs.size()!=expected.state_source_refs.size())fail("ResourceRecoveryReceipt2OriginalMismatch",actual.site_id);
 for(std::size_t i=0;i<actual.intensive_receipt_refs.size();++i)
  if(!same_source_ref(actual.intensive_receipt_refs[i],expected.intensive_receipt_refs[i]))fail("ResourceRecoveryReceipt2OriginalMismatch",actual.site_id);
 for(std::size_t i=0;i<actual.state_source_refs.size();++i)
  if(!same_source_ref(actual.state_source_refs[i].source_ref,expected.state_source_refs[i].source_ref)||
     actual.state_source_refs[i].source_schema!=expected.state_source_refs[i].source_schema)fail("ResourceRecoveryReceipt2OriginalMismatch",actual.site_id);
 return common_success();
 }catch(const Failure&f){return {false,{},f.error};}}
}
