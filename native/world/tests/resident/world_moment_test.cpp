// WorldMoment/2真实值运算回归；不创建世界、推进器或领域完成结果。
#include "hanmo/resident/world_moment.hpp"
#include <iostream>
#include <cstdlib>
#include <new>
#include <limits>
#include <stdexcept>
#include <type_traits>
using namespace hanmo::resident;
namespace allocation_fault { int fail_after=-1; }
// 单次真实分配失败仅在目标测试显式开启，其后自动恢复正常new。
void* operator new(std::size_t bytes){if(allocation_fault::fail_after==0){allocation_fault::fail_after=-1;throw std::bad_alloc();}if(allocation_fault::fail_after>0)--allocation_fault::fail_after;if(auto* p=std::malloc(bytes?bytes:1))return p;throw std::bad_alloc();}
// 测试分配器严格配对释放，生产实现没有故障注入分支。
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
namespace {
// 无论构建是否关闭assert，合同不符都使测试失败。
void check(bool condition,const char* label){if(!condition)throw std::runtime_error(label);}
// 便捷构造仍经生产规范化入口，不用测试版分数或运算实现。
CanonicalFraction fraction(const char* n,const char* d){auto out=normalize_fraction(n,d);check(out.ok,"fraction construction");return out.value;}
// 比较生产冷编码的两字段，确保结果值及正规形式均一致。
void equals(const CanonicalFraction& actual,const char* n,const char* d){auto encoded=encode_canonical_fraction(actual);check(encoded.ok,"fraction encoding");check(encoded.value.numerator==n&&encoded.value.denominator==d,"exact canonical value");}
// 比较必须先成功，不能把失败默认的零结果当成相等。
int order(const Outcome<int>& result){check(result.ok,"exact comparison succeeded");return result.value;}
// 测试错误分类，失败不能伪装成功或领域不支持。
template<class T> void fails(const Outcome<T>& value,const char* code){check(!value.ok,"expected failure");check(value.error.original.code==code,"exact failure code");}
// 普通数、超64位分母、约分和非法词法只走同一数值叶。
void canonical_values(){
    equals(fraction("14","6"),"7","3");equals(fraction("0","184467440737095516160"),"0","1");
    equals(fraction("340282366920938463463374607431768211456","18446744073709551616"),"18446744073709551616","1");
    for(const char* invalid:{"","00","01","+1","-1"," 1","1 ","1e3","1.0"})fails(normalize_fraction(invalid,"1"),"InvalidExactNatural");
    fails(normalize_fraction("1","0"),"ZeroExactDenominator");
    fails(decode_canonical_fraction({"2","4"}),"NonCanonicalFraction");
    fails(decode_canonical_fraction({"0","2"}),"NonCanonicalFraction");
    auto large=decode_canonical_fraction({"1","340282366920938463463374607431768211507"});check(large.ok,"denominator beyond 128 bits");
    auto wire=encode_canonical_fraction(large.value);check(wire.ok,"large encode");auto decoded=decode_canonical_fraction(wire.value);check(decoded.ok,"large roundtrip");
    check(order(compare_fractions(decoded.value,large.value))==0,"large roundtrip equality");
}
// 规范化只在tick真实进位时查原Integer界，不能因中间乘积大而误拒。
void moment_arithmetic(){
    auto normalized=normalize_world_moment(12,fraction("7","3"));check(normalized.ok&&normalized.value.half_day_tick==14,"carry 7/3");equals(normalized.value.sub_half_day,"1","3");
    WorldMoment start{12,fraction("0","1")};auto first=add_world_duration(start,HalfDayDuration{fraction("1","8")});check(first.ok,"first slice");
    auto second=add_world_duration(first.value,HalfDayDuration{fraction("5","24")});check(second.ok,"second slice");equals(second.value.sub_half_day,"1","3");
    auto whole=add_world_duration(start,HalfDayDuration{fraction("1","3")});check(whole.ok,"whole slice");check(order(compare_world_moments(whole.value,second.value))==0,"split equivalence");
    auto elapsed=elapsed_world_duration(start,second.value);check(elapsed.ok,"elapsed");equals(elapsed.value.value,"1","3");
    fails(elapsed_world_duration(second.value,start),"NegativeWorldDuration");
    auto zero=elapsed_world_duration(start,start);check(zero.ok,"zero duration");equals(zero.value.value,"0","1");
    fails(make_moment_span(start,start),"NonPositiveMomentSpan");check(make_moment_span(start,second.value).ok,"positive span");
    WorldMoment bad{0,fraction("1","1")};fails(compare_world_moments(bad,start),"NonCanonicalWorldMoment");
    fails(add_world_duration(bad,HalfDayDuration{}),"NonCanonicalWorldMoment");
    const auto max=std::numeric_limits<Integer>::max(),min=std::numeric_limits<Integer>::min();
    auto edge=normalize_world_moment(max,fraction("1","2"));check(edge.ok,"max tick fractional remains legal");
    fails(add_world_duration(edge.value,HalfDayDuration{fraction("1","2")}),"WorldMomentTickOverflow");
    auto full=normalize_world_moment(min,fraction("18446744073709551615","1"));check(full.ok&&full.value.half_day_tick==max,"full int64 carry without overflow");
    auto difference=elapsed_world_duration(WorldMoment{min,{}},WorldMoment{max,{}});check(difference.ok,"full int64 difference");equals(difference.value.value,"18446744073709551615","1");
    fails(normalize_world_moment(min,fraction("18446744073709551616","1")),"WorldMomentTickOverflow");
    auto borrow=elapsed_world_duration(WorldMoment{0,fraction("2","3")},WorldMoment{1,fraction("1","3")});check(borrow.ok,"fraction borrow");equals(borrow.value.value,"2","3");
}
// 量纲只经显式N换算，旧offset不是半日分子；没有N不能猜迁移。
void scale_and_legacy(){
    static_assert(!std::is_convertible_v<Integer,WorldMoment>);static_assert(!std::is_convertible_v<HalfDayDuration,WorkUnits>);
    auto work=duration_to_scale_units(HalfDayDuration{fraction("1","3")},128);check(work.ok,"to scale");equals(work.value.value,"128","3");
    auto duration=scale_units_to_duration(work.value,128);check(duration.ok,"from scale");equals(duration.value.value,"1","3");
    fails(duration_to_scale_units(HalfDayDuration{},0),"InvalidTimeScale");fails(scale_units_to_duration(WorkUnits{},-1),"InvalidTimeScale");
    auto legacy=embed_legacy_world_moment(12,43,128);check(legacy.ok,"legacy embedding");equals(legacy.value.sub_half_day,"43","128");
    fails(embed_legacy_world_moment(12,128,128),"InvalidLegacyWorldOffset");fails(embed_legacy_world_moment(12,-1,128),"InvalidLegacyWorldOffset");fails(embed_legacy_world_moment(12,0,0),"InvalidTimeScale");
}
// 冷读严格拒绝非正规字节，writer不把坏Moment悄悄进位修复。
void codec_and_order(){
    auto decoded=decode_world_moment({7,{"1","3"}});check(decoded.ok,"moment decoding");auto encoded=encode_world_moment(decoded.value);check(encoded.ok&&encoded.value.half_day_tick==7,"moment encoding");check(encoded.value.sub_half_day.numerator=="1"&&encoded.value.sub_half_day.denominator=="3","moment cold fields");
    fails(decode_world_moment({7,{"3","3"}}),"NonCanonicalFraction");fails(decode_world_moment({7,{"4","3"}}),"NonCanonicalWorldMoment");
    fails(encode_world_moment({7,fraction("4","3")}),"NonCanonicalWorldMoment");
    auto a=WorldMoment{7,fraction("18446744073709551616","18446744073709551617")};auto b=WorldMoment{7,fraction("18446744073709551617","18446744073709551618")};
    check(order(compare_world_moments(a,b))<0,"comparison exceeds int64 cross product");check(order(compare_world_moments(b,a))>0,"reverse comparison");check(order(compare_world_moments(WorldMoment{6,{}},a))<0,"tick primary comparison");
}
// 第二字段分配失败必须保留赋值前完整值，不能留下未约分或错误的半份分数。
void copy_allocation_failure(){
    auto original=fraction("1","2");auto replacement=fraction("18446744073709551616","18446744073709551617");
    for(int fail_index:{0,1}){bool caught=false;allocation_fault::fail_after=fail_index;try{original=replacement;}catch(const std::bad_alloc&){caught=true;}allocation_fault::fail_after=-1;check(caught,"copy assignment allocation failure injected");equals(original,"1","2");auto wire=encode_canonical_fraction(original);check(wire.ok&&decode_canonical_fraction(wire.value).ok,"failed assignment preserves codec invariant");equals(replacement,"18446744073709551616","18446744073709551617");}
    original=replacement;equals(original,"18446744073709551616","18446744073709551617");
    original=original;equals(original,"18446744073709551616","18446744073709551617");
}
// 聚合复制必须将真实分配异常交给候选上层捕获，不能被错误noexcept推导终止进程。
void aggregate_allocation_failure(){
    static_assert(!std::is_nothrow_copy_assignable_v<CanonicalFraction>);
    static_assert(!std::is_nothrow_copy_assignable_v<WorldMoment>);
    static_assert(!std::is_nothrow_copy_assignable_v<HalfDayDuration>);
    static_assert(!std::is_nothrow_copy_assignable_v<WorkUnits>);
    static_assert(!std::is_nothrow_copy_assignable_v<MomentSpan>);
    const WorldMoment replacement{15,fraction("18446744073709551616","18446744073709551617")};
    for(int fail_index:{0,1}){WorldMoment original{12,fraction("1","2")};bool caught=false;allocation_fault::fail_after=fail_index;try{original=replacement;}catch(const std::bad_alloc&){caught=true;}allocation_fault::fail_after=-1;check(caught,"aggregate allocation failure remains catchable");equals(original.sub_half_day,"1","2");}
}
// 工作预算是技术失败而非非法大分母；预算提高后同输入仍能精确完成。
void resource_budget(){
    ExactArithmeticBudget small;small.max_limbs=1;small.max_steps=1000;
    auto limited=normalize_fraction("18446744073709551616","3",small);fails(limited,"ExactArithmeticBudgetExceeded");check(limited.error.kind==ErrorKind::internal,"budget classified technical");
    equals(fraction("18446744073709551616","3"),"18446744073709551616","3");
    ExactArithmeticBudget retry;retry.max_limbs=16;auto retried=normalize_fraction("18446744073709551616","3",retry);check(retried.ok,"same value succeeds with adequate trusted budget");equals(retried.value,"18446744073709551616","3");
    WorldMoment original{12,fraction("1","3")};auto failed_add=add_world_duration(original,HalfDayDuration{fraction("1","2")},small);fails(failed_add,"ExactArithmeticBudgetExceeded");check(original.half_day_tick==12,"failed arithmetic preserves input tick");equals(original.sub_half_day,"1","3");
    small.max_limbs=16;small.max_steps=0;fails(normalize_fraction("1","2",small),"ExactArithmeticBudgetExceeded");
}
}
// 明确输出目标验证范围，失败保原原因，不声称真实世界或K2完整交付。
int main(){try{canonical_values();moment_arithmetic();scale_and_legacy();codec_and_order();copy_allocation_failure();aggregate_allocation_failure();resource_budget();std::cout<<"PASS WorldMoment/2 exact value, scale, codec, boundary and budget contracts\n";return 0;}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}
