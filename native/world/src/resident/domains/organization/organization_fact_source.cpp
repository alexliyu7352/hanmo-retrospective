#include "organization_fact_source.hpp"
namespace hanmo::resident {
Outcome<StableId> append_organization_fact_in_candidate(CommonStore& c,OrganizationState& s,OrganizationFact fact){if(fact.occurred_at){auto canonical=encode_world_moment(*fact.occurred_at);if(!canonical.ok)return {false,{},canonical.error};if(fact.tick!=fact.occurred_at->half_day_tick)return {false,{},common_failure("OrganizationFactClockMismatch").error};}auto id=c.next_id("organization");if(!id.ok)return id;fact.id=id.value;auto result=c.append_fact({id.value,fact.kind,"organization"});if(!result.ok)return {false,{},result.error};if(!s.facts.insert(id.value,std::move(fact)))return {false,{},common_failure("DuplicateOrganizationFact",id.value).error};return id;}
}
