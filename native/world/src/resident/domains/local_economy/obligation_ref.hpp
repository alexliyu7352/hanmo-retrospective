// E05/E20.5: identity of an original owner's obligation, never a second ledger.
#pragma once
#include "resident/kernel/source_ref.hpp"
namespace hanmo::resident {
struct ObligationRef {
 StableId owner_domain;
 StableId obligation_id;
};
// Actual fiscal/LE obligations belong to local_economy; Task and commercial
// contracts remain with tasks/commerce. The owner resolves the original key and
// rejects missing or ambiguous rows. This reference grants no payment authority.
template<class Archive,class Value>void obligation_ref_fields(Archive&archive,Value&value) {
 archive("owner_domain",value.owner_domain);archive("obligation_id",value.obligation_id);
}
Status validate_obligation_ref_shape(const ObligationRef&);
bool same_obligation_ref(const ObligationRef&,const ObligationRef&);
class CodecVisitor;
class ProjectionWriter;
struct BoundaryValue;
Status visit_obligation_ref(CodecVisitor&,ObligationRef&);
Status emit_obligation_ref(ProjectionWriter&,const StableId&name,const ObligationRef&);
Outcome<BoundaryValue> encode_obligation_ref(const ObligationRef&);
Outcome<ObligationRef> decode_obligation_ref(const BoundaryValue&);
}
