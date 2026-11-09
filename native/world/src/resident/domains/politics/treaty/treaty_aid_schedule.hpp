// M-PERFORMANCE's original schedule row. Financial obligations remain S5-owned;
// this is a borrowed schedule projection, not a second cash/commodity ledger.
#pragma once
#include "treaty_terms_contract.hpp"
namespace hanmo::resident {
struct TreatyAidSchedule {
 StableId id,treaty_id;Integer terms_revision=0;StableId term_id;
 SourceRef activation_ref,schedule_terms_ref;
 Integer next_occurrence_ordinal=0;std::optional<SourceRef> termination_ref;
};
// Every reference below is to a true applied M-TREATY row. The terms source is
// the original offer/counteroffer revision, not a synthesized per-term Fact.
struct ActivatedTreatyAidTermsRead {
 StableId treaty_id;Integer terms_revision=0;StableId term_id;
 const TreatyTermsV1* original_terms=nullptr;
 const TreatyMaterialAidTerm* original_term=nullptr;
 SourceRef original_terms_source,activation_source;WorldMoment activated_at;
 std::optional<SourceRef> termination_source;
 std::optional<WorldMoment> terminated_at;
 // Runtime original-owner projection; never copied into old persisted /1 rows.
 Integer activation_source_schema=1,termination_source_schema=1,schedule_source_schema=1;
};
class TreatyAidTermsOriginalRead {public:virtual ~TreatyAidTermsOriginalRead()=default;
 // Must validate actual ratifications/activation and immutable revision identity;
 // must not call the performance/schedule validator back recursively.
 virtual Outcome<ActivatedTreatyAidTermsRead> read_activated_material_aid_terms(
  const StableId& treaty_id,Integer revision,const StableId& term_id)const=0;
};
enum class TreatyAidDueKind {future,due,all_established,term_ended};
struct TreatyAidDueRead {
 TreatyAidDueKind kind=TreatyAidDueKind::future;Integer ordinal=0;
 std::optional<Integer> due_tick,delivery_deadline_tick;
 SourceRef activation_ref;std::optional<SourceRef> termination_ref;
};
// Pure scheduling read. It never skips a missed due or changes the cursor. The
// owning M-PERFORMANCE validator separately proves the full established prefix.
Outcome<TreatyAidDueRead> read_treaty_aid_due(
 const TreatyAidSchedule&,const TreatyAidTermsOriginalRead&,const OrganizationAuthorityClock&);
Status validate_treaty_aid_activation_terms(const ActivatedTreatyAidTermsRead&);
Status visit_treaty_aid_schedule(CodecVisitor&,TreatyAidSchedule&);
Status emit_treaty_aid_schedule_projection(ProjectionWriter&,std::string_view,const TreatyAidSchedule&);
Outcome<BoundaryValue> encode_treaty_aid_schedule(const TreatyAidSchedule&);
Outcome<TreatyAidSchedule> decode_treaty_aid_schedule(const BoundaryValue&);
}
