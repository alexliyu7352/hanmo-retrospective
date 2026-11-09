// 原游戏360日/720半日历法的唯一精确读适配；数字来自原family规则，不成为作者可改新玩法。
#pragma once
#include "calendar_gate.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/domains/actor_foundation/actor_foundation_refs.hpp"
namespace hanmo::resident {
using CalendarRuleRef=ActorFoundationRuleRef;
inline constexpr const char* original_calendar_rule_id="original_360_day_calendar";
struct CalendarYearBoundary {
    CheckpointMemoryCharge memory;
    CalendarRuleRef profile_ref;Integer game_year_ordinal=0,from_tick=0,to_tick=0;
    CalendarYearBoundary()=default;
    CalendarYearBoundary(const CalendarRuleRef&,Integer year,Integer from,Integer to);
    CalendarYearBoundary(const CalendarYearBoundary&);
    CalendarYearBoundary(CalendarYearBoundary&&)noexcept=default;
    CalendarYearBoundary& operator=(const CalendarYearBoundary&);
    CalendarYearBoundary& operator=(CalendarYearBoundary&&)noexcept;
    void swap(CalendarYearBoundary&)noexcept;
};
class OriginalCalendarRead {
    // 只读值可长于admit调用；普通字符串/数组费用与该实际值共同拥有。
    CheckpointMemoryCharge memory_;
    CatalogIdentity identity_;CalendarRuleRef rule_;
    OriginalCalendarRead(const CatalogIdentity&,const CalendarRuleRef&);
public:
    // compose登记该原规则ref后创建；不接受自由days/year、原点偏置或替代日历脚本。
    static Outcome<OriginalCalendarRead> admit(const CatalogIdentity&,const CalendarRuleRef&);
    OriginalCalendarRead()=default;
    OriginalCalendarRead(const OriginalCalendarRead&)=default;
    OriginalCalendarRead(OriginalCalendarRead&&)noexcept=default;
    OriginalCalendarRead& operator=(const OriginalCalendarRead&);
    OriginalCalendarRead& operator=(OriginalCalendarRead&&)noexcept;
    void swap(OriginalCalendarRead&)noexcept;
    void retain_checkpoint_memory(CheckpointRetentionCensus&)const;
    const CalendarRuleRef& profile_ref()const noexcept{return rule_;}
    Outcome<Integer> year_ordinal(Integer at_tick)const;
    Outcome<std::optional<CalendarYearBoundary>> year_boundary(Integer from_tick,Integer to_tick)const;
    Outcome<WorldMoment> next_year_boundary(const WorldMoment&,ExactArithmeticBudget={})const;
    // 两种真实age_basis保持分型；Declared不产生birth_tick，ExactBirth不改其真实生日。
    Outcome<Integer> declared_age_at(Integer years,Integer as_of_tick,Integer original_year_ordinal,const WorldMoment&,ExactArithmeticBudget={})const;
    Outcome<Integer> exact_birth_age_at(Integer birth_tick,const WorldMoment&,ExactArithmeticBudget={})const;
private:
    Status valid()const;
};
}
