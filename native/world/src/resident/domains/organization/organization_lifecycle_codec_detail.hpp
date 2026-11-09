// B1.1同一字段表供JSON、binary与冻结投影；写侧只借读，所有赋值仅在reading分支。
#pragma once
#include "organization_lifecycle_contract.hpp"
#include "resident/kernel/exact_struct_codec.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
namespace hanmo::resident { struct VerifiedOfficeTransition; }
namespace hanmo::resident::organization_lifecycle_fields {
template<class C> Status source(C& c,SourceRef& s){c.field("domain",s.domain);c.field("kind",s.kind);c.field("id",s.id);return c.status().ok&&!valid_source_ref(s)?c.reject("InvalidOrganizationRuntimeSource"):c.status();}
template<class C> Status sources(C& c,const StableId& name,std::vector<SourceRef>& s){return c.sequence(name,s,[](auto& v,auto& x){return source(v,x);});}
template<class C> Status strings(C& c,const StableId& name,std::vector<StableId>& s){return c.sequence(name,s,[](auto& v,auto& x){return v.field("value",x);});}
template<class C,class R> Status rule(C& c,R& r){c.field("id",r.id);c.field("version",r.version);c.field("catalog_digest",r.catalog_digest);return c.status().ok&&(r.id.empty()||r.version<1||r.catalog_digest.empty())?c.reject("InvalidOrganizationLifecycleRule"):c.status();}
// 七标签wire遵循原具名字符串，绝不以C++序号持久化。
template<class C> Status lifecycle(C& c,OrganizationStatus& s){StableId tag;if(!c.reading()){const auto* value=organization_lifecycle_tag(s);if(!value)return c.reject("InvalidOrganizationLifecycleTag");tag=value;}c.field("value",tag);if(!c.status().ok)return c.status();auto parsed=organization_lifecycle_from_tag(tag);if(!parsed)return c.reject("UnknownOrganizationLifecycleTag");if(c.reading())s=*parsed;return c.status();}
template<class C,class E,std::size_t N> Status named(C& c,E& e,const char* const (&names)[N]){StableId tag;if(!c.reading()){auto i=static_cast<std::size_t>(e);if(i>=N)return c.reject("InvalidOrganizationRuntimeTag");tag=names[i];}c.field("value",tag);if(!c.status().ok)return c.status();std::size_t i=0;for(;i<N&&tag!=names[i];++i){}if(i==N)return c.reject("UnknownOrganizationRuntimeTag");if(c.reading())e=static_cast<E>(i);return c.status();}
template<class C> Status terminal(C& c,OrganizationTerminalReason& e){static constexpr const char* names[]={"formation_abandoned","merged_into_successor","dissolved_by_decision","continuity_exhausted","legacy_dissolved"};return named(c,e,names);}
template<class C> Status cause(C& c,OrganizationLifecycleCause& e){static constexpr const char* names[]={"formation_registered","formation_completed","institutional_limit_applied","institutional_limit_released","merge_accepted","merge_abandoned","merge_completed","exile_continuity_applied","restoration_applied","dissolution_accepted","continuity_failed","dissolution_completed"};return named(c,e,names);}
// 枚举直接占原字段字符串槽，Optional在JSON仍是字符串或null，不额外套value对象。
template<class C> Status lifecycle_field(C& c,const StableId& name,OrganizationStatus& value){StableId tag;if(!c.reading()){const auto* p=organization_lifecycle_tag(value);if(!p)return c.reject("InvalidOrganizationLifecycleTag");tag=p;}c.field(name,tag);if(!c.status().ok)return c.status();auto parsed=organization_lifecycle_from_tag(tag);if(!parsed)return c.reject("UnknownOrganizationLifecycleTag");if(c.reading())value=*parsed;return c.status();}
template<class C> Status optional_lifecycle(C& c,const StableId& name,std::optional<OrganizationStatus>& value){std::optional<StableId> tag;if(!c.reading()&&value){const auto* p=organization_lifecycle_tag(*value);if(!p)return c.reject("InvalidOrganizationLifecycleTag");tag=p;}c.optional(name,tag,[](auto& v,auto& x){return v.field("value",x);});if(c.reading()&&c.status().ok){if(!tag)value.reset();else{auto parsed=organization_lifecycle_from_tag(*tag);if(!parsed)return c.reject("UnknownOrganizationLifecycleTag");value=*parsed;}}return c.status();}
template<class C,class E,std::size_t N> Status enum_field(C& c,const StableId& name,E& value,const char* const (&tags)[N]){StableId text;if(!c.reading()){auto n=static_cast<std::size_t>(value);if(n>=N)return c.reject("InvalidOrganizationRuntimeTag");text=tags[n];}c.field(name,text);if(!c.status().ok)return c.status();std::size_t n=0;for(;n<N&&text!=tags[n];++n){}if(n==N)return c.reject("UnknownOrganizationRuntimeTag");if(c.reading())value=static_cast<E>(n);return c.status();}
template<class C> Status optional_terminal(C& c,const StableId& name,std::optional<OrganizationTerminalReason>& value){static constexpr const char* tags[]={"formation_abandoned","merged_into_successor","dissolved_by_decision","continuity_exhausted","legacy_dissolved"};std::optional<StableId> text;if(!c.reading()&&value){auto n=static_cast<std::size_t>(*value);if(n>=5)return c.reject("InvalidOrganizationTerminalReason");text=tags[n];}c.optional(name,text,[](auto& v,auto& x){return v.field("value",x);});if(c.reading()&&c.status().ok){if(!text)value.reset();else{std::size_t n=0;for(;n<5&&*text!=tags[n];++n){}if(n==5)return c.reject("UnknownOrganizationTerminalReason");value=static_cast<OrganizationTerminalReason>(n);}}return c.status();}
template<class C> Status checkpoint(C& c,CheckpointIdentity& i){StableId hex;if(!c.reading()){static constexpr char d[]="0123456789abcdef";for(auto b:i.world_id){hex.push_back(d[b>>4]);hex.push_back(d[b&15]);}}c.field("world_id",hex);if(!c.status().ok)return c.status();if(hex.size()!=i.world_id.size()*2)return c.reject("InvalidOrganizationMigrationWorld");if(c.reading())for(std::size_t n=0;n<i.world_id.size();++n){auto digit=[](char x){return x>='0'&&x<='9'?x-'0':x>='a'&&x<='f'?x-'a'+10:-1;};auto a=digit(hex[n*2]),b=digit(hex[n*2+1]);if(a<0||b<0)return c.reject("InvalidOrganizationMigrationWorld");i.world_id[n]=static_cast<std::uint8_t>(a*16+b);}c.field("revision",i.revision);c.field("tick",i.tick);return c.object("catalog",[&](auto& v){return exact_fields::catalog(v,i.catalog);});}
template<class C> Status seed(C& c,LifecycleSeedPolicy& r){c.field("id",r.id);c.field("version",r.version);c.field("organization_kind",r.organization_kind);lifecycle_field(c,"initial_lifecycle",r.initial_lifecycle);optional_terminal(c,"terminal_reason",r.terminal_reason);strings(c,"required_office_ids",r.required_office_ids);c.optional("base_right_ref",r.base_right_ref,[](auto& v,auto& x){return source(v,x);});sources(c,"mobile_carrier_refs",r.mobile_carrier_refs);return sources(c,"initial_responsibility_refs",r.initial_responsibility_refs);}
template<class C> Status metadata(C& c,OrganizationMetadata& r){c.field("organization_id",r.organization_id);c.field("revision",r.revision);c.object("origin_ref",[&](auto& v){return source(v,r.origin_ref);});return c.optional("last_lifecycle_transition_ref",r.last_lifecycle_transition_ref,[](auto& v,auto& x){return source(v,x);});}
template<class C> Status origin(C& c,OrganizationLifecycleOrigin& r){
 c.field("id",r.id);c.field("organization_id",r.organization_id);
 return c.object("origin",[&](auto& v){
  auto tag=r.origin.index();
  {
   StableId kind;if(!v.reading())kind=tag==0?"authored_seed":tag==1?"founded":"legacy_migration";
   v.field("kind",kind);if(!v.status().ok)return v.status();
   if(kind=="authored_seed")tag=0;else if(kind=="founded")tag=1;else if(kind=="legacy_migration")tag=2;else return v.reject("UnknownOrganizationLifecycleOrigin");
  }
  if(tag==0){
   if(v.reading()){v.template prepare_default_values<OrganizationAuthoredOrigin>();if(!v.status().ok)return v.status();r.origin.emplace<OrganizationAuthoredOrigin>();}
   auto& a=std::get<OrganizationAuthoredOrigin>(r.origin);v.object("catalog_identity",[&](auto& x){return exact_fields::catalog(x,a.catalog_identity);});v.field("organization_row_id",a.organization_row_id);return v.object("lifecycle_seed_policy_ref",[&](auto& x){return rule(x,a.lifecycle_seed_policy_ref);});
  }
  if(tag==1){
   if(v.reading()){v.template prepare_default_values<OrganizationFoundedOrigin>();if(!v.status().ok)return v.status();r.origin.emplace<OrganizationFoundedOrigin>();}
   auto& a=std::get<OrganizationFoundedOrigin>(r.origin);v.object("founding_decision_ref",[&](auto& x){return source(x,a.founding_decision_ref);});return v.object("registration_result_ref",[&](auto& x){return source(x,a.registration_result_ref);});
  }
  if(tag==2){
   if(v.reading()){v.template prepare_default_values<OrganizationMigratedOrigin>();if(!v.status().ok)return v.status();r.origin.emplace<OrganizationMigratedOrigin>();}
   auto& a=std::get<OrganizationMigratedOrigin>(r.origin);v.object("source_checkpoint_identity",[&](auto& x){return checkpoint(x,a.source_checkpoint_identity);});v.field("original_profile",a.original_profile);v.field("original_organization_id",a.original_organization_id);v.field("original_status",a.original_status);return v.object("migration_decision_ref",[&](auto& x){return source(x,a.migration_decision_ref);});
  }
  return v.reject("UnknownOrganizationLifecycleOrigin");
 });
}
template<class C> Status migration(C& c,OrganizationLifecycleMigrationDecision& r){c.field("id",r.id);c.object("source_checkpoint_identity",[&](auto& v){return checkpoint(v,r.source_checkpoint_identity);});c.field("original_profile",r.original_profile);c.field("original_organization_id",r.original_organization_id);c.field("original_status",r.original_status);lifecycle_field(c,"selected_lifecycle",r.selected_lifecycle);optional_terminal(c,"selected_terminal_reason",r.selected_terminal_reason);return sources(c,"evidence_refs",r.evidence_refs);}
template<class C> Status transition(C& c,OrganizationLifecycleTransition& r){c.field("id",r.id);c.field("organization_id",r.organization_id);c.field("before_revision",r.before_revision);c.field("after_revision",r.after_revision);optional_lifecycle(c,"before_status",r.before_status);lifecycle_field(c,"after_status",r.after_status);{static constexpr const char* tags[]={"formation_registered","formation_completed","institutional_limit_applied","institutional_limit_released","merge_accepted","merge_abandoned","merge_completed","exile_continuity_applied","restoration_applied","dissolution_accepted","continuity_failed","dissolution_completed"};enum_field(c,"cause_kind",r.cause_kind,tags);}c.object("cause_ref",[&](auto& v){return source(v,r.cause_ref);});c.optional("outcome_ref",r.outcome_ref,[](auto& v,auto& x){return source(v,x);});c.object("effective_moment",[&](auto& v){return exact_fields::moment(v,r.effective_moment);});return optional_terminal(c,"terminal_reason",r.terminal_reason);}
template<class C> Status outcome(C& c,OrganizationLifecycleOutcome& r){c.field("id",r.id);strings(c,"old_org_ids",r.old_org_ids);c.optional("new_org_id",r.new_org_id,[](auto& v,auto& x){return v.field("value",x);});sources(c,"member_transition_refs",r.member_transition_refs);sources(c,"control_transition_refs",r.control_transition_refs);sources(c,"asset_receipts",r.asset_receipts);sources(c,"responsibility_dispositions",r.responsibility_dispositions);{static constexpr const char* names[]={"formed","split","merged","exile","dissolving","terminal"};enum_field(c,"status",r.status,names);}return c.object("cause_ref",[&](auto& v){return source(v,r.cause_ref);});}
template<class C> Status disposition(C& c,OrganizationTerminalDisposition& r){c.field("id",r.id);c.field("organization_id",r.organization_id);c.object("lifecycle_outcome_ref",[&](auto& v){return source(v,r.lifecycle_outcome_ref);});return sources(c,"responsibility_disposition_refs",r.responsibility_disposition_refs);}
// Only these existing original rows contain fallible exact-number moves.
// The canonical table owner precharges the destination shell and keeps staged
// storage alive. Failure abandons the entire unpublished decode candidate.
template<class T>inline constexpr bool exact_table_row=std::is_same_v<T,OrganizationLifecycleTransition>||std::is_same_v<T,VerifiedOfficeTransition>||std::is_same_v<T,AuthorityBasis>;
struct OrganizationExactDecodedRowTransfer {
 template<class Destination,class T>Status operator()(Destination& destination,std::pair<StableId,T>& row)const{
  static_assert(exact_table_row<T>,"Only original Organization exact rows may use this transfer");
  destination.emplace_back(std::move(row));return common_success();
 }
};
template<class T,class F>Status table(CodecVisitor& c,const StableId& name,OrderedTable<T>& rows,F f){
 if constexpr(exact_table_row<T>)return common_checkpoint::table(c,name,rows,f,OrganizationExactDecodedRowTransfer{});
 else return common_checkpoint::table(c,name,rows,f);
}
template<class T,class F>Status table(ActorFoundationJsonVisitor& c,const StableId& name,OrderedTable<T>& rows,F f){
 if constexpr(exact_table_row<T>)return c.table(name,rows,f,OrganizationExactDecodedRowTransfer{});
 else return c.table(name,rows,f);
}
template<class T,class F>Status table(ActorFoundationProjectionArchive& c,const StableId& name,OrderedTable<T>& rows,F f){return c.table(name,rows,f);}
}
