#pragma once
#include "population_count_revision.hpp"
#include "resident/domains/organization/organization_model.hpp"
#include "resident/domains/travel/group_transport_registration.hpp"
#include "resident/domains/military/population_military.hpp"
namespace hanmo::resident {
struct PopulationChangeTimeContext {
 const CommonStore& common;const LocalEconomyState& economy;const PopulationTransportState& population;
 const OrganizationState& organizations;const TransportRegistrationState& transport;
 const CurrentMomentRead& clock;const PopulationMilitaryReadContext* military=nullptr;
};
struct PopulationOriginalChangeTime {
 SourceRef population_source;MomentOwnerSource actual_owner_source;WorldMoment occurred_at;
};
// Read only the actual immutable owner of an existing P02 revision. A legacy
// count tick is never promoted to a made-up exact Moment.
Outcome<PopulationOriginalChangeTime> read_population_original_change_time(
 const PopulationChangeTimeContext&,const PopulationRevisionSource&);
}
