#pragma once
#include "military_local_motion_prediction.hpp"
#include "military_roster_history.hpp"
#include "naval_assets.hpp"
namespace hanmo::resident {
struct MilitaryTroopFootprintRef {StableId troop_type_id;};
struct MilitaryHullFootprintRef {StableId hull_id;};
using MilitaryBodyDefinitionRef=std::variant<MilitaryTroopFootprintRef,MilitaryHullFootprintRef>;
// S2 3.2's authored finite rational polygon. Offsets are in the named chart's
// axial units at original heading zero. Content version is not a source schema.
// No implicit radius, member-count scaling or mutable center is permitted.
struct MilitaryBodyFootprintDefinition2 {
    StableId id;Integer version=0;StableId geometry_ref;
    MilitaryBodyDefinitionRef original_body_definition;
    std::vector<MilitarySpatialPoint> offsets;
};
struct MilitaryBodyFootprintCatalog2 {
    OrderedTable<MilitaryBodyFootprintDefinition2> definitions;
};
Status validate_military_body_footprint_catalog(const MilitaryBodyFootprintCatalog2&,
    const MilitaryBattleCatalog&,const MilitarySpatialOriginalSourceRead&,
    const NavalAssetCatalog* actual_naval_catalog=nullptr,ExactArithmeticBudget budget={});
Outcome<const MilitaryBodyFootprintDefinition2*> read_military_body_footprint_definition(
    const MilitaryBodyFootprintCatalog2&,const SourceRef&,Integer source_schema,
    ExactArithmeticBudget budget={});
struct MilitaryBodyFootprintReadContext {
    const MilitaryLocalMotionPredictionContext& motion;
    const MilitaryCatalog& military_catalog;
    const MilitaryBodyFootprintCatalog2& footprints;
    const PopulationMilitaryState* population=nullptr;
    const PopulationMilitaryCatalog* population_catalog=nullptr;
    const MilitaryFormationSourceRead* formations=nullptr;
    const MilitaryReturnSourceRead* returns=nullptr;
    const ExactTrainingPeriodRead* periods=nullptr;
    const NavalAssetCatalog* naval_catalog=nullptr;
    const NavalAssetState* naval_assets=nullptr;
};
struct MilitaryActualBodyFootprintRead {
    const MilitaryBodyFootprintDefinition2* actual_definition=nullptr;
    MilitaryLocalBodyMotionPrediction actual_motion;
    MomentOwnerSource original_shape;
    std::vector<MilitarySpatialPoint> oriented_offsets;
    std::vector<MomentOwnerSource> original_roster_sources;
    SourceRef original_entry_snapshot;
    // Vessel bodies select a locked original hull, independently of changing
    // crew membership/title. The source-valid actual body entry is still
    // mandatory through actual_motion's session and spatial readers.
    const NavalVesselDefinition* original_vessel=nullptr;
};
struct MilitarySessionBodyFootprintsRead {
    const DynamicMilitarySessionCurrent2* actual_session=nullptr;
    MomentSpan common_window;
    std::vector<MilitaryActualBodyFootprintRead> bodies;
};
// Actual land bodies select their shape from the original roster troop and
// actual chart, never a caller's polygon, unit count, leader or display sprite.
// This finite land-only entry rejects VesselBody. The generic entry below uses
// the locked original vessel identity, never an arbitrary unit on that ship.
// Actual aboard/entry eligibility remains in the mandatory session source read.
Outcome<MilitarySessionBodyFootprintsRead> read_actual_land_session_body_footprints(
    const MilitaryBodyFootprintReadContext&,const StableId& session,Integer generation,
    const WorldMoment& requested_upper);
Outcome<MilitarySessionBodyFootprintsRead> read_actual_session_body_footprints(
    const MilitaryBodyFootprintReadContext&,const StableId& session,Integer generation,
    const WorldMoment& requested_upper);
struct MilitaryBodyContactPairPrediction {
    std::size_t first_body_ordinal=0,second_body_ordinal=0;
    MilitaryContactParameterWindow closed_window;
    CanonicalFraction first_inward_parameter;
};
struct MilitaryBodyContactPrediction {
    MilitarySessionBodyFootprintsRead original_inputs;
    std::optional<WorldMoment> first_contact_at;
    std::vector<MilitaryBodyContactPairPrediction> simultaneous_pairs;
};
using MilitaryLandBodyContactPrediction=MilitaryBodyContactPrediction;
// Complete body/body candidate enumeration from the above actual source set.
// Null means no inward body contact within this exact piece only. It does not
// attest terrain/portal legality or a transmission NoContactThrough. Existing
// initial penetration needs the real entry co-location/deployment exception;
// this ordinary branch rejects it rather than granting a permanent exception.
// This never applies BlockedAtContact or creates a battle/contact source.
Outcome<MilitaryLandBodyContactPrediction> predict_actual_land_body_contacts(
    const MilitaryBodyFootprintReadContext&,const StableId& session,Integer generation,
    const WorldMoment& requested_upper);
// Same exact set intersection for actual UnitBody or VesselBody records.
// Naval identity does not grant boarding, operation or combat eligibility.
Outcome<MilitaryBodyContactPrediction> predict_actual_military_body_contacts(
    const MilitaryBodyFootprintReadContext&,const StableId& session,Integer generation,
    const WorldMoment& requested_upper);
struct MilitaryBodyDomainContactPrediction {
    std::size_t body_ordinal=0;
    MilitaryContactParameterWindow closed_inside_window;
    CanonicalFraction first_outward_parameter;
};
struct MilitarySessionDomainContactPrediction {
    MilitarySessionBodyFootprintsRead original_inputs;
    std::optional<WorldMoment> first_contact_at;
    std::vector<MilitaryBodyDomainContactPrediction> simultaneous_boundaries;
};
// All actual session bodies against their real chart domains. This returns a
// boundary gate, not a blocking/exit permission, contact source or NoContact
// for interior terrain. An already straddling body needs its actual portal
// admission/exit proof; this ordinary branch cannot assume that exception.
Outcome<MilitarySessionDomainContactPrediction> predict_actual_military_domain_contacts(
    const MilitaryBodyFootprintReadContext&,const StableId& session,Integer generation,
    const WorldMoment& requested_upper);
struct MilitarySessionSpatialGatePrediction {
    // One source-valid input set, shared by every candidate below. No second
    // read of current/heading/rate can silently alter a comparison's premise.
    MilitarySessionBodyFootprintsRead original_inputs;
    std::optional<WorldMoment> first_gate_at;
    std::vector<MilitaryBodyContactPairPrediction> simultaneous_pairs;
    std::vector<MilitaryBodyDomainContactPrediction> simultaneous_boundaries;
    std::vector<std::size_t> completed_edge_body_ordinals;
};
// Composes all real body pairs, chart-domain boundaries and earliest edge dues
// from exactly one original source read. Equal M retains every candidate in
// its original canonical body order. This remains a prediction: interior
// forbidden terrain, actual portal/entry exceptions and other input changes
// still require their original owners before a prefix/contact may be adopted.
Outcome<MilitarySessionSpatialGatePrediction> predict_actual_military_body_domain_and_edge_gates(
    const MilitaryBodyFootprintReadContext&,const StableId& session,Integer generation,
    const WorldMoment& requested_upper);
}
