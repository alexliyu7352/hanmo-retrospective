#pragma once
#include "self_transport_funding.hpp"
namespace hanmo::resident::self_transport_funding_fields {
template<class C>void fields(C&c,SourceRef&v){c("domain",v.domain);c("kind",v.kind);c("id",v.id);}
template<class C>void fields(C&c,WorldMoment&v){c("half_day_tick",v.half_day_tick);c("sub_half_day",v.sub_half_day);}
template<class C>void fields(C&c,SelfTransportFundingTerms&v){
 c("id",v.id);c("principal_actor_id",v.principal_actor_id);c("wallet_id",v.wallet_id);
 c("journey_or_plan_ref",v.journey_or_plan_ref);c("currency_id",v.currency_id);c("gross_cap",v.gross_cap);
 c("valid_from",v.valid_from);c("until",v.until);c("allowed_destination_scope",v.allowed_destination_scope);
 c("continuity",v.continuity);c("acceptance_ref",v.acceptance_ref);c("revision",v.revision);
 auto shape=validate_self_transport_funding_terms_shape(v);if(!shape.ok)c.reject(shape.error.original.code,shape.error.original.path);
}
template<class C>void fields(C&c,PrivateTransportFundingState&v){c("schema_version",v.schema_version);if(v.schema_version!=1){c.reject("UnsupportedPrivateTransportFundingVersion","private_transport_funding");return;}c("terms",v.terms);}
}
