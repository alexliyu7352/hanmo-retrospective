// Shared original physical-reference fields. Existing physical codec and S9
// nested adopted references use these exact callbacks.
#pragma once
#include "economic_facility_sources.hpp"
namespace hanmo::resident::facility_codec {
template<class A>void record(A& a,FacilitySourceRef& v){a("source_ref",v.source_ref);a("source_schema",v.source_schema);}
template<class A>void record(A& a,FacilityClaimReceiptRef& v){a("slot_ref",v.slot_ref);a("claim_set_revision",v.claim_set_revision);}
template<class A>void record(A& a,FacilityActivationReceiptRef& v){a("receipt_id",v.receipt_id);a("source_schema",v.source_schema);}
template<class A>void record(A& a,InstalledPhysicalUnitSource& v){a("source_ref",v.source_ref);a("source_schema",v.source_schema);}
template<class A>void record(A& a,ActivatedPhysicalUnitSource& v){a("original_facility_activation_receipt_ref",v.original_facility_activation_receipt_ref);}
template<class A>void record(A& a,FacilityPhysicalUnitRef& v){a("facility_id",v.facility_id);a("installation_source_ref",v.installation_source_ref);a("unit_key",v.unit_key);}
}
