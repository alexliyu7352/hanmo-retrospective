// 对应原family_rules.cpp::year及actor_age_read.hpp::ticks_per_year；新精度不会加快岁龄/年检。
#include "original_calendar.hpp"
#include "world_copy_plan.hpp"
#include "world_retention.hpp"
#include <cstdint>
#include <limits>
#include <stdexcept>
namespace hanmo::resident {
static_assert(std::is_nothrow_move_constructible_v<CatalogIdentity> && std::is_nothrow_move_assignable_v<CatalogIdentity>);
static_assert(std::is_nothrow_move_constructible_v<CalendarRuleRef> && std::is_nothrow_move_assignable_v<CalendarRuleRef>);
namespace {
constexpr Integer original_half_days_per_year=720;
template<class T>Outcome<T> fail(const char* code){return {false,{},common_failure(code,"original_calendar").error};}
}
namespace {
CheckpointMemoryCharge reserve_calendar_rule(const CalendarRuleRef& rule){return CheckpointMemoryCharge(0);}
CheckpointMemoryCharge reserve_calendar_read(const CatalogIdentity& identity,const CalendarRuleRef& rule){return CheckpointMemoryCharge(0);}
}
CalendarYearBoundary::CalendarYearBoundary(const CalendarRuleRef& rule,Integer year,Integer from,Integer to)
    :memory(reserve_calendar_rule(rule)),profile_ref(rule),game_year_ordinal(year),from_tick(from),to_tick(to){}
CalendarYearBoundary::CalendarYearBoundary(const CalendarYearBoundary& other):CalendarYearBoundary(other.profile_ref,other.game_year_ordinal,other.from_tick,other.to_tick){}
void CalendarYearBoundary::swap(CalendarYearBoundary& other)noexcept{memory.swap(other.memory);using std::swap;swap(profile_ref,other.profile_ref);swap(game_year_ordinal,other.game_year_ordinal);swap(from_tick,other.from_tick);swap(to_tick,other.to_tick);}
CalendarYearBoundary& CalendarYearBoundary::operator=(const CalendarYearBoundary& other){if(this!=&other){CalendarYearBoundary fresh(other);swap(fresh);}return *this;}
CalendarYearBoundary& CalendarYearBoundary::operator=(CalendarYearBoundary&& other)noexcept{if(this!=&other)swap(other);return *this;}
OriginalCalendarRead::OriginalCalendarRead(const CatalogIdentity& identity,const CalendarRuleRef& rule)
    :memory_(reserve_calendar_read(identity,rule)),identity_(identity),rule_(rule){}
void OriginalCalendarRead::swap(OriginalCalendarRead& other)noexcept{memory_.swap(other.memory_);using std::swap;swap(identity_,other.identity_);swap(rule_,other.rule_);}
OriginalCalendarRead& OriginalCalendarRead::operator=(const OriginalCalendarRead& other){if(this!=&other){OriginalCalendarRead fresh(other);swap(fresh);}return *this;}
OriginalCalendarRead& OriginalCalendarRead::operator=(OriginalCalendarRead&& other)noexcept{if(this!=&other)swap(other);return *this;}
void OriginalCalendarRead::retain_checkpoint_memory(CheckpointRetentionCensus& census)const{census.resource(memory_.resource());}
Status OriginalCalendarRead::valid()const{if(rule_.id!=original_calendar_rule_id||rule_.version!=1||rule_.catalog_digest.empty()||rule_.catalog_digest!=identity_.content_digest||identity_.ruleset_id.empty())return common_failure("OriginalCalendarRuleNotAdmitted");return common_success();}
Outcome<OriginalCalendarRead> OriginalCalendarRead::admit(const CatalogIdentity& identity,const CalendarRuleRef& rule){try{OriginalCalendarRead result(identity,rule);auto checked=result.valid();if(!checked.ok)return {false,{},std::move(checked.error)};return {true,std::move(result),{}};}catch(const std::bad_alloc&){return {false,{},common_failure("CalendarResourceExhausted","original_calendar",ErrorKind::internal).error};}catch(const std::length_error&){return {false,{},common_failure("CalendarResourceExhausted","original_calendar",ErrorKind::internal).error};}}
Outcome<Integer> OriginalCalendarRead::year_ordinal(Integer at)const{auto checked=valid();if(!checked.ok)return {false,{},std::move(checked.error)};if(at<0)return fail<Integer>("InvalidCalendarTick");return {true,at/original_half_days_per_year,{}};}
Outcome<std::optional<CalendarYearBoundary>> OriginalCalendarRead::year_boundary(Integer from,Integer to)const{auto a=year_ordinal(from);if(!a.ok)return {false,{},std::move(a.error)};auto z=year_ordinal(to);if(!z.ok)return {false,{},std::move(z.error)};if(from==std::numeric_limits<Integer>::max()||to!=from+1)return fail<std::optional<CalendarYearBoundary>>("CalendarAdjacentStepRequired");if(z.value==a.value)return {true,std::nullopt,{}};if(z.value-a.value!=1||to%original_half_days_per_year!=0)return fail<std::optional<CalendarYearBoundary>>("CalendarYearBoundaryRequired");return {true,CalendarYearBoundary{rule_,z.value,from,to},{}};}
Outcome<WorldMoment> OriginalCalendarRead::next_year_boundary(const WorldMoment& current,ExactArithmeticBudget b)const{auto encoded=encode_world_moment(current,b);if(!encoded.ok)return {false,{},std::move(encoded.error)};auto year=year_ordinal(current.half_day_tick);if(!year.ok)return {false,{},std::move(year.error)};if(year.value>=std::numeric_limits<Integer>::max()/original_half_days_per_year)return fail<WorldMoment>("CalendarYearBoundaryOverflow");return {true,WorldMoment{(year.value+1)*original_half_days_per_year,{}},{}};}
Outcome<Integer> OriginalCalendarRead::declared_age_at(Integer years,Integer as_of,Integer original_year,const WorldMoment& at,ExactArithmeticBudget b)const{auto encoded=encode_world_moment(at,b);if(!encoded.ok)return {false,{},std::move(encoded.error)};auto baseline=year_ordinal(as_of);if(!baseline.ok)return baseline;auto now=year_ordinal(at.half_day_tick);if(!now.ok)return now;if(years<0||baseline.value!=original_year||at.half_day_tick<as_of||now.value<original_year)return fail<Integer>("InvalidDeclaredCalendarAgeBasis");const auto added=now.value-original_year;if(years>std::numeric_limits<Integer>::max()-added)return fail<Integer>("CalendarAgeOverflow");return {true,years+added,{}};}
Outcome<Integer> OriginalCalendarRead::exact_birth_age_at(Integer birth,const WorldMoment& at,ExactArithmeticBudget b)const{auto checked=valid();if(!checked.ok)return {false,{},std::move(checked.error)};auto encoded=encode_world_moment(at,b);if(!encoded.ok)return {false,{},std::move(encoded.error)};if(at.half_day_tick<0||birth>at.half_day_tick)return fail<Integer>("InvalidExactBirthAgeBasis");const auto elapsed=static_cast<std::uint64_t>(at.half_day_tick)-static_cast<std::uint64_t>(birth);const auto years=elapsed/static_cast<std::uint64_t>(original_half_days_per_year);if(years>static_cast<std::uint64_t>(std::numeric_limits<Integer>::max()))return fail<Integer>("CalendarAgeOverflow");return {true,static_cast<Integer>(years),{}};}
}
