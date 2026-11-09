// D1.1: original S6 references to S5-established obligations. No financial
// balance, Shipment state or second scheduling cursor belongs to this row.
#pragma once
#include "treaty_aid_schedule.hpp"
#include "resident/domains/local_economy/obligation_ref.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
struct TreatyAidOccurrenceBinding {
 StableId id;SourceRef schedule_ref;Integer occurrence_ordinal=0;
 SourceRef establishment_result_ref;ObligationRef original_obligation_ref;
 std::optional<SourceRef> shipment_ref;std::vector<SourceRef> receipt_result_refs;
 std::optional<SourceRef> deadline_result_ref,termination_disposition_ref;
};
// Borrowed original financial result, resolved by the S5 owner. Implementations
// must read the actual hold conversion/obligation; they must not call S6 prefix
// validation back or accept an obligation inferred from this binding.
struct TreatyAidEstablishmentRead {
 SourceRef establishment_result_ref,schedule_ref,activation_ref,schedule_terms_ref;
 Integer occurrence_ordinal=0,due_tick=0,delivery_deadline_tick=0;
 ObligationRef original_obligation_ref;WorldMoment established_at;
};
class TreatyAidOccurrenceOriginalRead {public:
 virtual ~TreatyAidOccurrenceOriginalRead()=default;
 virtual Outcome<TreatyAidEstablishmentRead> read_material_aid_establishment(
  const SourceRef& actual_result)const=0;
 // Enumerates the actual S5 rows for this schedule, including retained sources;
 // a list derived from the S6 bindings is not an original-source implementation.
 virtual Outcome<std::vector<SourceRef>> enumerate_material_aid_establishments(
  const SourceRef& schedule)const=0;
 virtual Status validate_material_aid_shipment(const TreatyAidEstablishmentRead&,
  const SourceRef& shipment)const=0;
 virtual Status validate_material_aid_receipt(const TreatyAidEstablishmentRead&,
  const std::optional<SourceRef>& shipment,const SourceRef& receipt)const=0;
 virtual Status validate_material_aid_deadline_result(const TreatyAidEstablishmentRead&,
  const SourceRef& actual_result)const=0;
 virtual Status validate_material_aid_termination_disposition(const TreatyAidEstablishmentRead&,
  const SourceRef& actual_result)const=0;
};
struct TreatyAidPerformanceReadContext {
 const CommonStore& common;
 const OrderedTable<TreatyAidSchedule>& schedules;
 const OrderedTable<TreatyAidOccurrenceBinding>& occurrence_bindings;
 const TreatyAidTermsOriginalRead& terms;
 const TreatyAidOccurrenceOriginalRead& finances;
 const OrganizationAuthorityClock& clock;
};
SourceRef treaty_aid_schedule_source(const TreatyAidSchedule&);
SourceRef treaty_aid_occurrence_source(const TreatyAidOccurrenceBinding&);
Status validate_treaty_aid_occurrence_shape(const TreatyAidOccurrenceBinding&);
// Reconstructs the exact established prefix in both directions. It never
// repairs a cursor or recreates a missing occurrence during cold loading.
Status validate_treaty_aid_occurrence_prefixes(const TreatyAidPerformanceReadContext&);
Outcome<const TreatyAidOccurrenceBinding*> read_treaty_aid_occurrence_binding(
 const TreatyAidPerformanceReadContext&,const SourceRef& binding_source,Integer source_schema);
// The binding is adopted synchronously with the original establishment. Its
// exact moment is re-read from that actual S5 result; no duplicate time field.
Outcome<WorldMoment> read_treaty_aid_occurrence_source_moment(
 const TreatyAidPerformanceReadContext&,const SourceRef& binding_source,Integer source_schema);
// Internal same-candidate adoption AFTER the actual S5 establishment. The
// enclosing original establishment operation must also install the original
// promise-episode binding and publish all owners atomically. Any failure means
// discarding that whole candidate, including S5 writes and consumed IDs.
Outcome<SourceRef> adopt_treaty_aid_occurrence_in_candidate(
 CommonStore&,OrderedTable<TreatyAidSchedule>&,
 OrderedTable<TreatyAidOccurrenceBinding>&,const TreatyAidPerformanceReadContext&,
 const SourceRef& actual_establishment);
Status visit_treaty_aid_occurrence_binding(CodecVisitor&,TreatyAidOccurrenceBinding&);
Status emit_treaty_aid_occurrence_binding_projection(ProjectionWriter&,std::string_view,
 const TreatyAidOccurrenceBinding&);
Outcome<BoundaryValue> encode_treaty_aid_occurrence_binding(const TreatyAidOccurrenceBinding&);
Outcome<TreatyAidOccurrenceBinding> decode_treaty_aid_occurrence_binding(const BoundaryValue&);
}
