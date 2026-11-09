// Calendar facts keep their original integer meaning; only the trusted phase clock
// can expose a just-applied closing boundary before Common is normalized.
#pragma once
#include "resident/storage/common_store.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "hanmo/resident/world_moment.hpp"
namespace hanmo::resident {
inline Outcome<Integer> read_economic_calendar_tick(const CommonStore& common,const CurrentMomentRead* clock){
 if(!clock)return {true,common.tick(),{}};
 auto actual=clock->read_current_moment();if(!actual.ok)return {false,{},actual.error};
 auto valid=encode_world_moment(actual.value);if(!valid.ok)return {false,{},valid.error};
 if(actual.value.half_day_tick<common.tick())return {false,{},common_failure("EconomicPhaseClockBeforePublishedWorld").error};
 return {true,actual.value.half_day_tick,{}};
}
}
