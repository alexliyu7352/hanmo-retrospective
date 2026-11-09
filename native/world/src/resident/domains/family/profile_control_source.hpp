#pragma once
#include "profile_control_eligibility.hpp"
#include <array>
#include <string_view>
namespace hanmo::resident {
struct ProfileControllerSourceRegistration {
 std::string_view domain,kind;Integer source_schema=0;
};
// Finite original payload identities. Legacy1..4 changes consent sources, not
// its activation record. Control2's original activation is first source1.
// This list alone proves neither payload existence nor authority.
inline constexpr std::array<ProfileControllerSourceRegistration,6>
 profile_controller_source_registrations{{
  {"family","family_character_creation",1},
  {"family","actor_creation",2},
  {"family","family_succession",1},
  {"family","family_voluntary_succession",1},
  {"family_legacy","family_legacy_control_activated",1},
  {"control_continuation","control_activated",1}
 }};
struct ProfileControllerActualSource {
 const FactRecord* original_header=nullptr;
 const StableId* actor_id=nullptr;
 Integer source_schema=0;
};
// Mechanical control-at-source projection, using actual retained payloads,
// selected module identity and the original control chain. Full Family /
// Legacy / Continuation source validation remains a mandatory world gate.
// Catalog-authored initial control has no fabricated Fact SourceRef here.
Outcome<ProfileControllerActualSource> read_profile_controller_actual_source(
 const ProfileControlEligibilitySources&,const CatalogIdentity& trusted_catalog,
 const SourceRef& actual_source);
}
