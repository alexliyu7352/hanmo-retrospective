// Estate's original title and residence projection at an actual Growth
// boundary.
#pragma once
#include "estate_rest_adoption.hpp"
#include "estate_model.hpp"
#include "employment/estate_employment.hpp"
#include "resident/domains/growth/rest_estate_view.hpp"
#include "resident/domains/family/family_model.hpp"
#include "resident/kernel/read_stamp.hpp"
namespace hanmo::resident {
struct EstateRestHistoryContext {
 const CommonStore& common;
 const EstateCatalog& estates;
 const FamilyState& family;
 const AuthoredActorFoundationState& foundation;
 const ActorFoundationCatalog& foundation_catalog;
 const CurrentMomentRead& clock;
 const EstateEmploymentState* employment=nullptr;
};
struct EstateRestHistoricalRead {
 RestEstateView view;
 // Original S4 title applications retained by this adopted input. Initial title
// is the real ScenarioOrigin plus the immutable estate row, not a fabricated fact.
 std::vector<SourceRef> title_sources;
 std::vector<SourceRef> residence_sources;
};
// Only the actual LongRest consumption writer calls capture. The returned value
// is stored in that same original period and rolled back with its resource leg.
Outcome<EstateRestAdoptionBasis> capture_estate_rest_adoption(const EstateRestHistoryContext&,const EstateState&,const StableId& estate_id);
// The caller must first resolve the basis from its real action/period source.
// This pure read never checks today's title, residents, stock, or food balance.
Outcome<EstateRestHistoricalRead> read_estate_rest_at_adoption(const EstateRestHistoryContext&,const EstateRestAdoptionBasis&);
Status validate_estate_rest_adopted_view(const EstateRestHistoryContext&,const EstateRestAdoptionBasis&,const RestEstateView& actual_frozen_view);
}
