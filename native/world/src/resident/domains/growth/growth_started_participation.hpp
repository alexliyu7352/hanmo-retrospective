// Original Growth start participation. No second occupation ledger.
#pragma once
#include "growth_fixed_start.hpp"
namespace hanmo::resident {
struct GrowthStartedParticipationRead {
 const FactRecord* original_header=nullptr;
 const LifeFixedServiceBoundary* started=nullptr;
 const LifeFixedActorInputs* participant=nullptr;
};
// Only the four original payload families and their real, sealed start. The
// complete original participant set is checked before returning an optional
// member. A terminal Action need not still exist. No route/FSL/skill recursion.
Outcome<std::optional<GrowthStartedParticipationRead>> read_growth_started_participation(
 const CommonStore&,const GrowthState&,const MomentOwnerSource& actual_business,
 const StableId& actor,const WorldMoment& actual_now);
// True Common header -> that same owner payload. Unknown headers are not a
// permissive success and cannot acquire schema 1 by their name alone.
Outcome<std::optional<GrowthStartedParticipationRead>> read_growth_started_participation_at_header(
 const CommonStore&,const GrowthState&,const FactRecord& actual_header,
 const StableId& actor,const WorldMoment& actual_now);
bool is_growth_original_start_header(const FactRecord&)noexcept;
class GrowthStartedParticipationWrite {
public:virtual ~GrowthStartedParticipationWrite()=default;
 virtual bool bound_to(const CommonStore&,const GrowthState&)const noexcept=0;
 virtual Status consume_actual_growth_start(const MomentOwnerSource&,
  const WorldMoment&)=0;
};
bool growth_start_participation_consumer_selected(const ActorFoundationCatalog&)noexcept;
// Same check belongs in the original prepare/admission before Action/resource
// writes; the actual start and seal tail repeat it defensively.
Status validate_growth_start_participation_writer(const CommonStore&,const GrowthState&,
 const ActorFoundationCatalog&,const GrowthStartedParticipationWrite*);
}
