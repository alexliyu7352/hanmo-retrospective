// 原mandates行唯一字段表；与旧DiplomaticMission/知识/接受槽均不混用。
#pragma once
#include "treaty_mandate_sources.hpp"
#include "treaty_revision_codec_fields.hpp"
namespace hanmo::resident::treaty_mandate_codec_fields {
template<class C>Status limits(C& c,NegotiationMandate& r){
 static constexpr const char* kinds[]={"truce","passage","mutual_defense","material_aid","submission"};
 treaty_terms_codec_fields::tags(c,"allowed_term_kinds",r.allowed_term_kinds,kinds);
 c.object("scope",[&](auto& v){return treaty_terms_codec_fields::scope(v,r.scope);});
 c.sequence("quantity_caps",r.quantity_caps,[](auto& v,auto& amount){v.field("item_id",amount.item_id);return v.field("quantity",amount.quantity);});
 c.optional("money_cap",r.money_cap,[](auto& v,auto& amount){v.field("currency_id",amount.currency_id);return v.field("amount",amount.amount);});
 c.object("window",[&](auto& v){return treaty_terms_codec_fields::window(v,r.window);});return c.field("may_sign",r.may_sign);
}
template<class C>Status fields(C& c,TreatyMandateSource& r){
 c.field("id",r.id);c.field("treaty_id",r.treaty_id);c.field("original_terms_revision",r.original_terms_revision);
 c.object("original_terms_source",[&](auto& v){return treaty_terms_codec_fields::source(v,r.original_terms_source);});
 c.object("issuer",[&](auto& v){return treaty_revision_codec_fields::acting(v,r.issuer);});
 c.field("envoy_actor_id",r.envoy_actor_id);c.object("mandate",[&](auto& v){return limits(v,r.mandate);});
 c.object("issued_at",[&](auto& v){return exact_fields::moment(v,r.issued_at);});
 if(c.status().ok){auto valid=validate_treaty_mandate_source_shape(r);if(!valid.ok)return c.reject(valid.error.original.code,valid.error.original.path);}return c.status();
}
}
