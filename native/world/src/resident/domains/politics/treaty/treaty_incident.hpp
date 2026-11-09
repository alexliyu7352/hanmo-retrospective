// P15.3 original shared responsibility reference and objective incident row.
// ObligationRef (S5 ledger identity) is deliberately a different original type.
#pragma once
#include "treaty_terms_contract.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
struct TreatyObligationRef {
 StableId owner_domain,contract_id,term_id;Integer terms_revision=0;
};
enum class TreatyIncidentKind {unnotified_incident,unable_to_perform,deliberate_breach};
struct TreatyIncident {
 StableId id,treaty_id;Integer terms_revision=0;StableId term_id;
 SourceRef source_fact_ref;Integer occurrence_tick=0;
 TreatyIncidentKind incident_kind=TreatyIncidentKind::unnotified_incident;
 StableId liable_party;std::vector<TreatyObligationRef> remaining_obligation_refs;
};
// Original owners classify the actual event. Deliberate breach requires the
// original explicit BreakTreatyAndOrder/known-content source; lack of an answer
// is not sufficient. This view is not a caller-supplied verdict or a new ledger.
struct TreatyIncidentOriginalView {
 StableId treaty_id;Integer terms_revision=0;StableId term_id;SourceRef actual_source;
 WorldMoment occurred_at;TreatyIncidentKind kind=TreatyIncidentKind::unnotified_incident;
 StableId liable_party;std::vector<TreatyObligationRef> remaining_obligations;
};
class TreatyIncidentOriginalRead {public:virtual ~TreatyIncidentOriginalRead()=default;
 // Implementations must validate immutable signed terms and the real source
// discriminator/payload (S2 attack, genuine due result, S5/S3 receipt or refusal).
// This must not derive an event from the incident row or from today's location.
 virtual Outcome<TreatyIncidentOriginalView> read_actual_treaty_incident(
  const StableId& treaty_id,Integer terms_revision,const StableId& term_id,
  const SourceRef& actual_source)const=0;
 // Historical existence at the event, not today's remaining balance. Settlement
// does not erase an original responsibility referenced by an old incident.
 virtual Status validate_incident_obligation_at_source(const TreatyObligationRef&,
  const TreatyIncidentOriginalView&)const=0;
};
struct TreatyIncidentReadContext {
 const CommonStore& common;const OrderedTable<TreatyIncident>& incidents;
 const TreatyIncidentOriginalRead& originals;const OrganizationAuthorityClock& clock;
};
bool same_treaty_obligation_ref(const TreatyObligationRef&,const TreatyObligationRef&);
Status validate_treaty_obligation_ref_shape(const TreatyObligationRef&);
Status validate_treaty_incident_shape(const TreatyIncident&);
SourceRef treaty_incident_source(const TreatyIncident&);
Outcome<const TreatyIncident*> read_treaty_incident(const TreatyIncidentReadContext&,
 const SourceRef&,Integer source_schema);
Outcome<WorldMoment> read_treaty_incident_source_moment(const TreatyIncidentReadContext&,
 const SourceRef&,Integer source_schema);
Status validate_treaty_incidents(const TreatyIncidentReadContext&);
// Internal consumer only. No relationship/renown change, compensation payment,
// termination or knowledge is produced here. Their actual original consumers
// follow in the same candidate when required by the selected source contract.
Outcome<SourceRef> record_treaty_incident_in_candidate(CommonStore&,
 OrderedTable<TreatyIncident>&,const TreatyIncidentReadContext&,
 const StableId& treaty_id,Integer terms_revision,const StableId& term_id,
 const SourceRef& actual_source);
Status visit_treaty_incident(CodecVisitor&,TreatyIncident&);
Status emit_treaty_incident_projection(ProjectionWriter&,std::string_view,const TreatyIncident&);
Outcome<BoundaryValue> encode_treaty_incident(const TreatyIncident&);
Outcome<TreatyIncident> decode_treaty_incident(const BoundaryValue&);
}
