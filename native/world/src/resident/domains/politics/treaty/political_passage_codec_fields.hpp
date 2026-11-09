// Original Decision/1 fields are unchanged. Decision/2 explicitly appends its
// original purpose, finite authority basis and requested window; all row exits use this one table.
#pragma once
#include <type_traits>
#include "political_passage_source.hpp"
#include "treaty_terms_codec_fields.hpp"
#include "resident/domains/travel/movement_execution_ref_codec_fields.hpp"
#include "resident/domains/travel/passage_requester_codec_fields.hpp"
#include "resident/domains/travel/passage_purpose_codec_fields.hpp"
namespace hanmo::resident::political_passage_codec_fields {
using treaty_terms_codec_fields::source;
using treaty_terms_codec_fields::sources;
template<class C>Status identity(C& c,WorldIdentity& v){
 StableId hex;constexpr char digits[]="0123456789abcdef";if(!c.reading())for(const auto b:v.world_id){hex.push_back(digits[b>>4]);hex.push_back(digits[b&15]);}
 c.field("world_id",hex);if(!c.status().ok)return c.status();if(hex.size()!=2*v.world_id.size())return c.reject("InvalidPoliticalPassageWorldId");
 if(c.reading())for(std::size_t i=0;i<v.world_id.size();++i){auto digit=[](char a){return a>='0'&&a<='9'?a-'0':a>='a'&&a<='f'?a-'a'+10:-1;};auto hi=digit(hex[2*i]),lo=digit(hex[2*i+1]);if(hi<0||lo<0)return c.reject("InvalidPoliticalPassageWorldId");v.world_id[i]=static_cast<std::uint8_t>(16*hi+lo);}
 c.field("execution_epoch",v.execution_epoch);c.field("revision",v.revision);return c.field("tick",v.tick);
}
template<class C>Status basis(C& c,PoliticalPassageAuthorityBasis& value){
 static constexpr const char* tags[]={"existing_original_source","activated_treaty_passage","treaty_safe_exit"};std::size_t index=0;auto valid=treaty_terms_codec_fields::variant_tag(c,value,tags,index);if(!valid.ok)return valid;
 if(index==0){if(c.reading())value=ExistingOriginalPassageBasis{};auto& original=std::get<ExistingOriginalPassageBasis>(value);c.field("policy_id",original.policy_id);c.field("policy_version",original.policy_version);return c.field("catalog_digest",original.catalog_digest);}
 if(index==1){if(c.reading())value=ActivatedTreatyPassageBasis{};auto& a=std::get<ActivatedTreatyPassageBasis>(value);c.object("activation_ref",[&](auto& v){return source(v,a.activation_ref);});return c.field("term_id",a.term_id);}
 if(c.reading())value=TreatySafeExitPassageBasis{};auto& a=std::get<TreatySafeExitPassageBasis>(value);c.field("binding_id",a.binding_id);c.field("policy_version",a.policy_version);c.object("entry_receipt_ref",[&](auto& v){return source(v,a.entry_receipt_ref);});sources(c,"current_manifest_successor_refs",a.current_manifest_successor_refs);return c.object("exit_until",[&](auto& v){return exact_fields::moment(v,a.exit_until);});
}
template<class C>Status decision(C& c,PoliticalPassageDecision& d,Integer selected_schema){
 if(selected_schema!=1&&selected_schema!=2)return c.reject("PoliticalPassageSourceSchemaUnsupported");if(!c.reading()&&d.read_stamp)return c.reject("PersistentPassageReadStampForbidden");
 c.object("identity",[&](auto& v){return identity(v,d.identity);});c.object("boundary",[&](auto& v){return passage_boundary_codec_fields::fields(v,d.boundary);});
 Integer subject=static_cast<Integer>(d.subject.kind);c.field("subject_kind",subject);if(c.status().ok&&(subject<0||subject>2))return c.reject("InvalidPoliticalPassageSubject");if(c.reading()&&c.status().ok)d.subject.kind=static_cast<SubjectKind>(subject);
 c.field("subject_id",d.subject.id);c.field("mode",d.mode);c.field("entry_tick",d.entry_tick);
 Integer admission=static_cast<Integer>(d.admission);c.field("admission",admission);if(c.status().ok&&(admission<0||admission>2))return c.reject("InvalidPoliticalPassageAdmission");if(c.reading()&&c.status().ok)d.admission=static_cast<PassageAdmission>(admission);
 c.optional("toll",d.toll,[](auto& v,auto& t){v.field("currency_id",t.currency_id);v.field("amount",t.amount);v.field("payee_wallet_id",t.payee_wallet_id);return v.object("tariff_source_ref",[&](auto& x){return source(x,t.tariff_source_ref);});});
 sources(c,"authority_basis_refs",d.authority_basis_refs);sources(c,"treaty_term_refs",d.treaty_term_refs);sources(c,"local_delivery_source_refs",d.local_delivery_source_refs);c.object("admission_source",[&](auto& v){return source(v,d.admission_source);});c.object("requester",[&](auto& v){return passage_requester_codec_fields::fields(v,d.requester);});c.field("source_schema_version",d.source_schema_version);c.field("visible_reason_code",d.visible_reason_code);
 if(!c.status().ok)return c.status();if(d.source_schema_version!=selected_schema)return c.reject("PoliticalPassageSelectedSourceSchemaMismatch");
 if(selected_schema==2){c.object("purpose_context",[&](auto& v){return passage_purpose_codec_fields::fields(v,d.purpose_context);});c.object("authority_basis",[&](auto& v){return basis(v,d.authority_basis);});c.object("requested_window",[&](auto& v){return treaty_terms_codec_fields::window(v,d.requested_window);});}
 else if(c.reading()){d.purpose_context=OrdinaryPassageContext{};d.authority_basis=ExistingOriginalPassageBasis{};d.requested_window={};}
 if(c.reading())d.read_stamp.reset();
 // retention只遍历真实持有字段；不为值验证生成临时精确数字编码。
 if constexpr(std::is_same_v<C,CodecVisitor>){}
 if(c.status().ok){auto valid=validate_political_passage_authority_payload(d);if(!valid.ok)return c.reject(valid.error.original.code);}return c.status();
}
template<class C>Status record(C& c,PoliticalPassageDecisionSource& r,Integer schema){
 c.field("id",r.id);c.object("execution",[&](auto& v){return movement_execution_ref_codec_fields::fields(v,r.execution);});c.object("carrier",[&](auto& v){return spatial_carrier_ref_codec_fields::fields(v,r.carrier);});c.field("entry_ordinal",r.entry_ordinal);c.field("composition_revision",r.composition_revision);c.field("manifest_revision",r.manifest_revision);c.object("occurred_at",[&](auto& v){return exact_fields::moment(v,r.occurred_at);});c.object("decision",[&](auto& v){return decision(v,r.decision,schema);});
 if constexpr(std::is_same_v<C,CodecVisitor>){}
 if(c.status().ok){auto valid=validate_political_passage_decision_source_time(r);if(!valid.ok)return c.reject(valid.error.original.code);}return c.status();
}
}
