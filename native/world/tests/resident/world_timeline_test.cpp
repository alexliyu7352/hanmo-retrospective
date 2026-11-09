// 精确附段数值回归；此门不冒充真实Growth完工或全世界K2集成验收。
#include "resident/kernel/world_timeline.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
// 测试不用可关闭assert，失败总是终止本目标。
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
// 所有测试值也由生产正规化构造，不带另一套运算实现。
CanonicalFraction f(const char* n,const char* d){auto r=normalize_fraction(n,d);check(r.ok,"fraction");return r.value;}
// 比较检查生产Outcome，而非失败默认值。
bool same(const WorldMoment& a,const WorldMoment& b){auto r=compare_world_moments(a,b);check(r.ok,"compare");return r.value==0;}
// 冷数字必须最简，证明跨片不取整。
void fraction_is(const CanonicalFraction& value,const char* n,const char* d){auto r=encode_canonical_fraction(value);check(r.ok&&r.value.numerator==n&&r.value.denominator==d,"exact fraction");}
// 保错误码稳定，技术失败和输入错误不会被当作成功推进。
template<class T> void failure(const Outcome<T>& r,const char* code){check(!r.ok&&r.error.original.code==code,"expected error");}
// 从1/3开工需完整一半日，跨边界只得2/3而不能先拿一份成果。
void fractional_fixed_duration(){
    TimeScaleRef scale{"authored",1,128};WorldMoment start{5,f("1","3")};
    auto entry=begin_fixed_elapsed("a","execution","rest-H1","cost",start,1,scale);check(entry.ok,"begin");
    check(same(std::get<ElapsedRunning>(entry.value.timing).completion_due_at,{6,f("1","3")}),"fractional due");
    auto slice=prepare_fixed_elapsed_slice(entry.value,{start,{6,{}}},scale);check(slice.ok&&!slice.value.due,"boundary is not due");fraction_is(slice.value.credited_units.value,"256","3");
    entry.value.credited_units=slice.value.credited_units;entry.value.last_settled_at=slice.value.last_settled_at;
    check(validate_fixed_elapsed_value(entry.value,{6,{}},scale).ok,"mid-period stable");
    auto projected=fixed_elapsed_remaining_ticks(entry.value,scale);check(projected.ok&&projected.value==1,"ceil remaining");
    auto end=prepare_fixed_elapsed_slice(entry.value,{{6,{}},{6,f("1","3")}},scale);check(end.ok&&end.value.due,"true due");fraction_is(end.value.credited_units.value,"128","1");
    entry.value.credited_units=end.value.credited_units;entry.value.last_settled_at=end.value.last_settled_at;
    failure(validate_fixed_elapsed_value(entry.value,{6,f("1","3")},scale),"FullFixedElapsedStillRunning");
    entry.value.timing=ElapsedTerminal{"owner-result"};check(validate_fixed_elapsed_value(entry.value,{6,f("1","3")},scale).ok,"terminal value shape");
    failure(prepare_fixed_elapsed_slice(entry.value,{{6,f("1","3")},{6,f("1","2")}},scale),"FixedElapsedNotRunning");
}
// 分片组合只改变技术过程；同一真实总时长得到相同credited和due。
void split_and_invalid(){
    TimeScaleRef scale{"scale",2,7};WorldMoment start{0,f("1","7")};auto entry=begin_fixed_elapsed("a","e","d","c",start,2,scale);check(entry.ok,"begin split");
    auto whole=prepare_fixed_elapsed_slice(entry.value,{start,{1,f("2","3")}},scale);check(whole.ok,"whole");
    auto first=prepare_fixed_elapsed_slice(entry.value,{start,{0,f("3","5")}},scale);check(first.ok,"first");
    auto split=entry.value;split.credited_units=first.value.credited_units;split.last_settled_at=first.value.last_settled_at;
    auto second=prepare_fixed_elapsed_slice(split,{split.last_settled_at,{1,f("2","3")}},scale);check(second.ok,"second");auto compare=compare_fractions(whole.value.credited_units.value,second.value.credited_units.value);check(compare.ok&&compare.value==0,"split invariance");
    failure(prepare_fixed_elapsed_slice(split,{start,{0,f("4","5")}},scale),"InvalidFixedElapsedTimeOrder");
    failure(prepare_fixed_elapsed_slice(entry.value,{start,{2,f("1","2")}},scale),"FixedElapsedCrossesDue");
    failure(prepare_fixed_elapsed_slice(entry.value,{start,start},scale),"NonPositiveMomentSpan");
    split.timing=ElapsedSuspended{"actual-stop-source"};check(validate_fixed_elapsed_value(split,{50,{}},scale).ok,"suspended not accrue");
    failure(prepare_fixed_elapsed_slice(split,{{50,{}},{51,{}}},scale),"FixedElapsedNotRunning");
    auto invalid=entry.value;invalid.required_units=WorkUnits{f("3","2")};failure(validate_fixed_elapsed_value(invalid,start,scale),"InvalidFixedRequiredUnits");
    invalid=entry.value;invalid.timing=ElapsedRunning{{2,{}}};failure(validate_fixed_elapsed_value(invalid,start,scale),"FixedElapsedDueMismatch");
}
// 原H*N范围和余量投影均checked，预算是可恢复技术错误。
void bounds_and_budget(){
    TimeScaleRef scale{"scale",1,128};failure(begin_fixed_elapsed("a","e","d","c",{},std::numeric_limits<Integer>::max(),scale),"FixedDurationWorkUnitsOverflow");
    failure(begin_fixed_elapsed("a","e","d","c",{std::numeric_limits<Integer>::max(),{}},1,scale),"WorldMomentTickOverflow");
    failure(fixed_duration_work_units(0,128),"InvalidFixedDuration");failure(fixed_duration_work_units(1,0),"InvalidTimeScale");
    auto sum=add_work_units({f("1","3")},{f("2","5")});check(sum.ok,"work sum");fraction_is(sum.value.value,"11","15");auto difference=subtract_work_units(sum.value,{f("2","5")});check(difference.ok,"work difference");fraction_is(difference.value.value,"1","3");
    failure(subtract_work_units({f("1","3")},{f("1","2")}),"NegativeWorkUnits");
    auto ceil=ceil_half_day_duration({f("9223372036854775806","1")});check(ceil.ok&&ceil.value==std::numeric_limits<Integer>::max()-1,"projection integer");
    failure(ceil_half_day_duration({f("92233720368547758071","10")}),"ExactIntegerProjectionOverflow");
    ExactArithmeticBudget budget;budget.max_steps=0;auto limited=begin_fixed_elapsed("a","e","d","c",{},1,scale,budget);failure(limited,"ExactArithmeticBudgetExceeded");check(limited.error.kind==ErrorKind::internal,"technical budget");
}
// timeline没有第二tick；结构门拒绝Ready非整界，真实provider源闭包另由WorldBundle验证。
void timeline_shape(){
    WorldTimelineState t;t.scale_ref={"scale",1,128};t.period_id="period0";check(validate_timeline_value(10,t).ok,"initial ready");t.sub_half_day=f("1","3");failure(validate_timeline_value(10,t),"ReadyTimelineNotBoundary");t.boundary_state=PeriodBoundaryState::open;check(validate_timeline_value(10,t).ok,"nullable source shape leaves selected-provider check to world");t.opening_receipt_ref="actual-opening";check(validate_timeline_value(10,t).ok,"open shape");auto current=read_world_moment(10,t);check(current.ok&&same(current.value,{10,f("1","3")}),"common tick plus offset");t.sub_half_day=f("4","3");failure(read_world_moment(10,t),"NonCanonicalWorldMoment");
}
}
// 本目标只验证真实新类型与纯运算，不生成假owner回执充当世界集成证明。
int main(){try{fractional_fixed_duration();split_and_invalid();bounds_and_budget();timeline_shape();std::cout<<"world_timeline value tests passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
