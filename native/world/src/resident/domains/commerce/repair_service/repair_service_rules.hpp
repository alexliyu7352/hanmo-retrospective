// The original RepairPolicy author references resolve only inside the locked
// selected catalog. These rules do not prove customer/provider assent.
#pragma once
#include "repair_service_model.hpp"
namespace hanmo::resident {
class ProjectionWriter;class CodecVisitor;
SourceRef repair_fee_policy_source(const StableId&);
SourceRef repair_custody_policy_source(const StableId&);
Status validate_repair_service_rule_catalog(const RepairServiceRuleCatalog&,const RepairPolicyCatalog&);
Outcome<const RepairServiceFeePolicy*> read_repair_fee_policy(const RepairServiceRuleCatalog&,const MomentOwnerSource&);
Outcome<const RepairServiceCustodyPolicy*> read_repair_custody_policy(const RepairServiceRuleCatalog&,const MomentOwnerSource&);
Status validate_repair_service_rule_terms(const RepairServiceRuleCatalog&,const RepairPolicyCatalog&,const RepairServiceQuotedTerms&);
Outcome<RepairServiceRuleCatalog> decode_repair_service_rule_catalog(const BoundaryValue&,const RepairPolicyCatalog&);
Outcome<BoundaryValue> encode_repair_service_rule_catalog(const RepairServiceRuleCatalog&);
Status emit_repair_service_rule_catalog(ProjectionWriter&,const StableId&,const RepairServiceRuleCatalog&);
Status visit_repair_service_rule_catalog(CodecVisitor&,RepairServiceRuleCatalog&);
}
