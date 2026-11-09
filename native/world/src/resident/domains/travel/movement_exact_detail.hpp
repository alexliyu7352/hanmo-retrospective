// S1内部精确辅助仅传播原数值错误；不引入第二大数或默认成功。
#pragma once
#include "movement_exact.hpp"
#include <new>
#include <stdexcept>
#include <charconv>
#include <limits>
#include <string_view>
namespace hanmo::resident::movement_detail {
struct Failure { RuleError error; };
template<class T> T take(Outcome<T> value){if(!value.ok)throw Failure{std::move(value.error)};return std::move(value.value);}
inline void take(Status value){if(!value.ok)throw Failure{std::move(value.error)};}
[[noreturn]] inline void fail(const StableId& code,const StableId& path={}){throw Failure{common_failure(code,path).error};}
template<class T,class Fn> Outcome<T> guard(Fn fn){try{return {true,fn(),{}};}catch(const Failure& e){return {false,{},e.error};}catch(const std::bad_alloc&){return {false,{},common_failure("MovementResourceExhausted",{},ErrorKind::internal).error};}catch(const std::length_error&){return {false,{},common_failure("MovementResourceExhausted",{},ErrorKind::internal).error};}}
template<class Fn> Status status(Fn fn){return guard<Done>([&]{fn();return Done{};});}
inline CanonicalFraction integer(Integer x,ExactArithmeticBudget b={}){
 if(x<0)fail("NegativeMovementQuantity");
 char digits[std::numeric_limits<Integer>::digits10+1];
 const auto written=std::to_chars(digits,digits+sizeof digits,x,10);
 if(written.ec!=std::errc{})throw std::bad_alloc();
 return take(normalize_fraction(std::string_view(digits,static_cast<std::size_t>(written.ptr-digits)),"1",b));
}
inline int compare(const CanonicalFraction& a,const CanonicalFraction& c,ExactArithmeticBudget b={}){return take(compare_fractions(a,c,b));}
inline bool same_moment(const WorldMoment& a,const WorldMoment& c,ExactArithmeticBudget b={}){return take(compare_world_moments(a,c,b))==0;}
inline CanonicalFraction add(const CanonicalFraction& a,const CanonicalFraction& c,ExactArithmeticBudget b={}){return take(add_fractions(a,c,b));}
inline CanonicalFraction sub(const CanonicalFraction& a,const CanonicalFraction& c,ExactArithmeticBudget b={}){return take(subtract_fractions(a,c,b));}
inline CanonicalFraction mul(const CanonicalFraction& a,const CanonicalFraction& c,ExactArithmeticBudget b={}){return take(multiply_fractions(a,c,b));}
inline CanonicalFraction div(const CanonicalFraction& a,const CanonicalFraction& c,ExactArithmeticBudget b={}){return take(divide_fractions(a,c,b));}
inline bool source_valid(const SourceRef& source){return !source.domain.empty()&&!source.kind.empty()&&!source.id.empty();}
inline bool same_source(const SourceRef& a,const SourceRef& b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
inline StableId execution_key(const MovementExecutionRef& x){return std::visit([](const auto& v)->StableId{using T=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<T,PersonalMovementExecution>)return "personal:"+v.action_id+":"+v.journey_id+":"+std::to_string(v.segment_ordinal);else if constexpr(std::is_same_v<T,GroupMovementExecution>)return "group:"+v.journey_id+":"+std::to_string(v.segment_ordinal);else if constexpr(std::is_same_v<T,CustodyExitMovementExecution>)return "custody:"+v.escape_action_id+":"+v.exit_terms_id+":"+std::to_string(v.segment_ordinal);else return "shore:"+v.landing_operation_id+":"+v.crossing_action_id+":"+std::to_string(v.segment_ordinal);},x);}
inline StableId basis_id(const MovementBasis& b){return std::visit([](const auto& x){return x.basis_ref;},b);}
inline CanonicalFraction basis_limit(const MovementBasis& x,ExactArithmeticBudget b={}){if(const auto* t=std::get_if<TerrainCostBasis>(&x))return t->parameter_limit.value;return integer(1,b);}
inline StableId execution_action(const MovementExecutionRef& x){if(const auto* p=std::get_if<PersonalMovementExecution>(&x))return p->action_id;return {};}
} // 所有异常只在同一次具名纯/候选调用中转Outcome，外层负责撤销完整候选。
