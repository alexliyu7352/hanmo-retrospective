// Exact original Campaign/Resignation adjunct. No second authority ledger.
#pragma once
#include "office_exact.hpp"
#include "resident/domains/interaction/interaction_role_history.hpp"
#include <string_view>
namespace hanmo::resident {
class CommonActualFactPrefixSink;
struct MilitaryAuthorityTimeContext {
 const CommonStore& common;const MilitaryState& military;
 const MilitaryActionTimeState& actions;const ActionElapsedStore& elapsed;
 ExactOfficeReadContext office;
};
struct MilitaryActualSourceWindow;
struct MilitaryAuthoritySourceContext {
 const CommonStore& common;const MilitaryState& military;
 const MilitaryActionTimeState& actions;const ActionElapsedStore& elapsed;
 ExactOfficeSourceReadContext office;Integer selected_authority_time=0;
 const MilitaryActualSourceWindow* window=nullptr;
};
Status validate_military_authority_source_context(const MilitaryAuthoritySourceContext&);
// Same role-value/place proof for the real Campaign/Resign and Office cuts.
// actual_head must be the already authenticated original occurrence. This
// neither replaces full Interaction nor establishes enabling-parent authority.
// Retain sink output only after the entire enclosing source read succeeds.
Outcome<OriginalMilitaryInteractionRoleBeforePrefix> validate_military_original_role_before_source(
 const CommonStore&,const ExactOfficeReadContext&,const FactRecord& actual_head,
 const InteractionContext&,std::string_view expected_service,
 CommonActualFactPrefixSink* retained_prefix=nullptr);
Outcome<OriginalMilitaryInteractionRoleBeforePrefix> validate_military_original_role_before_source(
 const CommonStore&,const ExactOfficeSourceReadContext&,const FactRecord& actual_head,
 const InteractionContext&,std::string_view expected_service,
 CommonActualFactPrefixSink* retained_prefix=nullptr);
// Invoke before original ID allocation. Selected /1 never falls back to tick.
Status validate_military_authority_time_write(const CommonRead&,const MilitaryState&,const MilitaryAccess&);
Outcome<std::optional<MilitaryCampaignAcceptanceTime1>> capture_military_campaign_acceptance(
 const CommonRead&,const MilitaryState&,const MilitaryAccess&,const StableId& campaign_id,
 const StableId& actor_id,const StableId& office_id,const InteractionContext&);
Outcome<std::optional<MilitaryOfficeResignationTime1>> capture_military_office_resignation(
 const CommonRead&,const MilitaryState&,const MilitaryAccess&,const StableId& resignation_id,
 const OfficeTerms&,const InteractionContext&);
// Source-local original identity, exact clock and actual appointment join.
// The selected complete route also proves the original role value/place at
// the strict preceding Common prefix. Arrival/consent remain separate.
// A sink borrows the same traversal; retain output only if the entire read
// succeeds. No sink means validation only, not source-retention closure.
Outcome<const CampaignRecord*> read_military_campaign_acceptance_source(
 const MilitaryAuthorityTimeContext&,const SourceRef&,Integer source_schema,
 CommonActualFactPrefixSink* retained_prefix=nullptr,
 OriginalMilitaryInteractionRoleBeforePrefix* role_output=nullptr);
Outcome<const CampaignRecord*> read_military_campaign_acceptance_source(
 const MilitaryAuthoritySourceContext&,const SourceRef&,Integer source_schema,
 CommonActualFactPrefixSink* retained_prefix=nullptr,
 OriginalMilitaryInteractionRoleBeforePrefix* role_output=nullptr);
Outcome<const MilitaryResignationFact*> read_military_office_resignation_source(
 const MilitaryAuthorityTimeContext&,const SourceRef&,Integer source_schema,
 CommonActualFactPrefixSink* retained_prefix=nullptr,
 OriginalMilitaryInteractionRoleBeforePrefix* role_output=nullptr);
Outcome<const MilitaryResignationFact*> read_military_office_resignation_source(
 const MilitaryAuthoritySourceContext&,const SourceRef&,Integer source_schema,
 CommonActualFactPrefixSink* retained_prefix=nullptr,
 OriginalMilitaryInteractionRoleBeforePrefix* role_output=nullptr);
// Replays real appointment/resignation/occupation and integration heads in
// strict actual Common order. Integration has only its original integer tick:
// it proves a disable at this cut, never an invented exact occurrence M.
// A null appointment means no holder at this original cut. Legacy appointment
// mutations still reject. The fold validates integration currentness only for
// the requested office; it is not an all-integration whole-state validator.
struct MilitaryOfficeAuthorityBeforeSource {const DynamicMilitaryActionFact2* appointment=nullptr;};
Outcome<MilitaryOfficeAuthorityBeforeSource> read_military_office_authority_before(
 const MilitaryAuthorityTimeContext&,const StableId& office_id,const SourceRef& actual_boundary,
 const WorldMoment& boundary_at);
Outcome<MilitaryOfficeAuthorityBeforeSource> read_military_office_authority_before(
 const MilitaryAuthoritySourceContext&,const StableId& office_id,const SourceRef& actual_boundary,
 const WorldMoment& boundary_at);
Status validate_military_authority_time_sources(const MilitaryAuthorityTimeContext&,Integer selected_schema);
Status validate_military_authority_time_sources(const MilitaryAuthoritySourceContext&,Integer selected_schema);
// Common ordinal only. The caller supplies an actual prefix, never a fake
// before-fact id or an invented occurrence M for an integer-only event.
Outcome<MilitaryOfficeAuthorityBeforeSource> read_military_office_authority_at_actual_prefix(
 const MilitaryAuthoritySourceContext&,const StableId& office_id,
 const CommonActualFactPrefix&,const WorldMoment& actual_now,
 const MilitaryActualSourceWindow* window=nullptr);

// Reuses the original selected write binding and validates the actual unpublished post-write state.
Status validate_military_authority_time_after_write(const CommonRead&,const MilitaryState&,const MilitaryAccess&);
Status collect_military_authority_time_pins(const MilitaryAuthorityTimeContext&,SourcePinSink&);
Status validate_military_authority_time_pin(const MilitaryAuthorityTimeContext&,const SourcePin&);
std::vector<RequiredSourceConsumerSlot> military_authority_time_consumer_slots();
Outcome<bool> military_authority_time_source_may_release(const MilitaryAuthorityTimeContext&,const SourceRef&,Integer source_schema);
// Bootstrap identity only for the finite actual acceptance/completion Common heads. Domain
// payload remains on its sole original owner; no historical provider recursion.
class ProjectionWriter;
Status validate_military_authority_header_source(const FactRecord&,const MilitaryState&,const MilitaryActionTimeState&,Integer authority_schema,Integer action_time_schema);
Status emit_military_authority_header_payload_fields(ProjectionWriter&,const FactRecord&,const MilitaryState&,const MilitaryActionTimeState&,Integer authority_schema,Integer action_time_schema);
}
