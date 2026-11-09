// 原Calendar截止只比较唯一Moment；开闭端点由原条款声明，本层不延长原deadline。
#pragma once
#include "hanmo/resident/world_moment.hpp"
namespace hanmo::resident {
inline Outcome<bool> calendar_deadline_accepts(Integer deadline_tick,bool inclusive,const WorldMoment& actual,ExactArithmeticBudget budget={}){if(deadline_tick<0){RuleError e;e.original={"InvalidCalendarDeadline","deadline"};return {false,false,std::move(e)};}auto compared=compare_world_moments(actual,WorldMoment{deadline_tick,{}},budget);if(!compared.ok)return {false,false,std::move(compared.error)};return {true,compared.value<0||(inclusive&&compared.value==0),{}};}
}
