// 舟行热写只借原Travel真实执行结果，所有写入属于同一World候选。
#pragma once
#include "resident/domains/military/naval_navigation.hpp"
#include "resident/domains/military/naval_navigation_apply.hpp"
namespace hanmo::resident {
struct WorldBundle;
Outcome<std::optional<NavalVoyageFact>> prepare_naval_voyage_world(const WorldBundle&,const StableId&,const TravelQuote&);
Status record_naval_voyage_world(WorldBundle&,const std::optional<NavalVoyageFact>&,const StableId& action_id);
Outcome<NavalNavigationStepFact> step_naval_navigation_world(WorldBundle&,const TerrainExecution&,const DynamicStepInput&);
Status release_disabled_naval_journeys(WorldBundle&,const DynamicStepInput&);
}
