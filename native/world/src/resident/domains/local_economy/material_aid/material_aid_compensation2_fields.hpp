#pragma once
#include "material_aid_compensation2.hpp"
#include "material_aid_fields.hpp"
namespace hanmo::resident::material_aid_fields {
template<class C>void fields(C&c,AidCompensationScope2&v){c("id",v.id);c("original_schedule_commitment",v.original_schedule_commitment);c("original_resource_acceptance",v.original_resource_acceptance);c("accepted_at",v.accepted_at);c("occurrence_hold_refs",v.occurrence_hold_refs);}
template<class C>void fields(C&c,AidCompensationHold2&v){c("id",v.id);c("scope_id",v.scope_id);c("occurrence_ordinal",v.occurrence_ordinal);c("disposition_ref",v.disposition_ref);}
template<class C>void fields(C&c,AidCompensationDisposition2&v){c("id",v.id);c("hold_id",v.hold_id);Integer kind=static_cast<Integer>(v.kind);c("kind",kind);if(kind<0||kind>2){c.reject("UnsupportedAidCompensationDisposition",v.id);return;}if(c.reading())v.kind=static_cast<AidCompensationDispositionKind2>(kind);c("original_cause",v.original_cause);c("occurred_at",v.occurred_at);c("original_obligation_id",v.original_obligation_id);c("original_arrear_id",v.original_arrear_id);}
template<class C>void fields(C&c,AidCompensationPayment2&v){c("id",v.id);c("disposition_id",v.disposition_id);c("paid_at",v.paid_at);}
template<class C>void fields(C&c,MaterialAidCompensationState2&v){c("scopes",v.scopes);c("holds",v.holds);c("dispositions",v.dispositions);c("payments",v.payments);}
template<class C>void fields(C&c,TreatyMaterialAidState2&v){c("schema_version",v.schema_version);if(v.schema_version!=2){c.reject("UnsupportedTreatyMaterialAidVersion","");return;}c("principal",v.principal);c("compensation",v.compensation);}
}
