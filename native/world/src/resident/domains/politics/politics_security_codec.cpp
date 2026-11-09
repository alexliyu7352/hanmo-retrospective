// /2保原治理四根字段序并尾追加两根；/1明确拒丢弃非空治安来源。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "politics_security.hpp"
#include "politics_governance_legacy_fields.hpp"
#include "resident/domains/organization/organization_lifecycle_codec_detail.hpp"
#include "resident/domains/growth/growth_skill_qualification_codec_fields.hpp"
namespace hanmo::resident {namespace {
using namespace organization_lifecycle_fields;
// 原记录仍由同一legacy字段表展开，避免另一份安民/民心协议。
template<class C,class T>Status legacy_value(C&,const StableId&,T&);
template<class C>struct LegacyBorrowedArchive {C& codec;template<class T>void operator()(const char* name,T& value){legacy_value(codec,name,value);}};
template<class C,class T>Status legacy_record(C& c,T& value){LegacyBorrowedArchive<C> archive{c};dynamic_economy_codec::record(archive,value);return c.status();}
template<class C,class T>Status legacy_value(C& c,const StableId& name,T& value){if constexpr(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>)return c.field(name,value);else if constexpr(dynamic_economy_codec::IsTable<T>::value)return table(c,name,value,[](auto& v,auto& r){return legacy_record(v,r);});else return c.object(name,[&](auto& v){return legacy_record(v,value);});}
template<class C>Status initial(C& c,JurisdictionSecurityInitial& x){c.field("jurisdiction_id",x.jurisdiction_id);c.object("original_jurisdiction_definition",[&](auto& v){return rule(v,x.original_jurisdiction_definition);});return c.field("security",x.security);}
template<class C>Status current(C& c,JurisdictionSecurityState& x){c.field("jurisdiction_id",x.jurisdiction_id);c.field("registration_fact_id",x.registration_fact_id);c.field("last_fact_id",x.last_fact_id);c.object("original_jurisdiction_definition",[&](auto& v){return rule(v,x.original_jurisdiction_definition);});c.field("security",x.security);return c.field("revision",x.revision);}
template<class C>Status legacy_skill(C& c,LegacyPatrolSecuritySkillSnapshot& x){c.field("actor_id",x.actor_id);c.field("skill_id",x.skill_id);c.field("experience",x.experience);c.field("level",x.level);return c.sequence("original_qualification_sources",x.original_qualification_sources,[](auto& v,auto& s){v.object("source_ref",[&](auto& a){return source(a,s.source_ref);});return v.field("source_schema",s.source_schema);});}
template<class C>Status skill(C& c,PatrolSecuritySkillSnapshot& value,Integer version){
 if(version==2){if(c.reading())value=LegacyPatrolSecuritySkillSnapshot{};auto* old=std::get_if<LegacyPatrolSecuritySkillSnapshot>(&value);if(!old)return c.reject("PoliticalGovernanceSkillRequiresPhysical3");return legacy_skill(c,*old);}
 if(version!=3)return c.reject("UnsupportedPoliticalGovernanceSkillSchema");if(c.reading())value=GrowthSkillQualificationV2{};
 auto* exact=std::get_if<GrowthSkillQualificationV2>(&value);if(!exact)return c.reject("PoliticalGovernanceExactSkillSourceRequired");return growth_skill_qualification_codec::fields(c,*exact);
}
template<class C>Status consequence(C& c,SecurityConsequence& x,Integer version){c.field("id",x.id);c.field("jurisdiction_id",x.jurisdiction_id);c.object("program_ref",[&](auto& v){return source(v,x.program_ref);});c.object("actual_work_result",[&](auto& v){return source(v,x.actual_work_result);});c.field("target_line_id",x.target_line_id);c.field("previous_fact_id",x.previous_fact_id);c.object("program_spec_ref",[&](auto& v){return rule(v,x.program_spec_ref);});c.optional("skill_at_completion",x.skill_at_completion,[&](auto& v,auto& s){return skill(v,s,version);});c.field("before",x.before);c.field("after",x.after);c.field("requested_delta",x.requested_delta);c.field("revision",x.revision);return c.object("applied_at",[&](auto& v){return exact_fields::moment(v,x.applied_at);});}
template<class C>Status state_fields(C& c,PoliticsGovernanceState& s,Integer version=2){legacy_record(c,s);table(c,"jurisdiction_security",s.jurisdiction_security,[](auto& v,auto& x){return current(v,x);});return table(c,"security_consequences",s.security_consequences,[&](auto& v,auto& x){return consequence(v,x,version);});}
template<class C>Status catalog_fields(C& c,PoliticalSecurityCatalog& s){Integer schema=1;c.field("schema_version",schema);if(!c.status().ok)return c.status();if(schema!=1)return c.reject("UnsupportedPoliticalSecurityCatalogSchema");c.field("initial_tick",s.initial_tick);return table(c,"initial_security",s.initial_security,[](auto& v,auto& x){return initial(v,x);});}
}
Status visit_politics_governance2(CodecVisitor& c,PoliticsGovernanceState& s){return state_fields(c,s);}
Status emit_politics_governance2_projection(ProjectionWriter& sink,std::string_view name,const PoliticsGovernanceState& s){return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,s,[](auto& c,auto& v){return state_fields(c,v);});}
Outcome<BoundaryValue> encode_politics_governance2(const PoliticsGovernanceState& s){BoundaryValue out;BoundaryProjectionWriter sink(out);auto valid=emit_politics_governance2_projection(sink,"",s);if(valid.ok)valid=sink.finish();return {valid.ok,std::move(out),valid.error};}
Outcome<PoliticsGovernanceState> decode_politics_governance2(const BoundaryValue& value){PoliticsGovernanceState out;ActorFoundationJsonVisitor reader(value);auto valid=state_fields(reader,out);if(valid.ok)valid=reader.finish();return {valid.ok,std::move(out),valid.error};}
// 所选目录沿同一原字段表借读发射；外层shape不构造目录值树。
Status emit_political_security_catalog_projection(ProjectionWriter& sink,std::string_view name,const PoliticalSecurityCatalog& s){return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,s,[](auto& c,auto& v){return catalog_fields(c,v);});}
Outcome<BoundaryValue> encode_political_security_catalog(const PoliticalSecurityCatalog& s){BoundaryValue out;BoundaryProjectionWriter sink(out);auto valid=emit_political_security_catalog_projection(sink,"",s);if(valid.ok)valid=sink.finish();return {valid.ok,std::move(out),valid.error};}
Outcome<PoliticalSecurityCatalog> decode_political_security_catalog(const BoundaryValue& value,const CatalogIdentity& identity){PoliticalSecurityCatalog out;out.identity=identity;ActorFoundationJsonVisitor reader(value);auto valid=catalog_fields(reader,out);if(valid.ok)valid=reader.finish();return {valid.ok,std::move(out),valid.error};}
Status visit_politics_governance3(CodecVisitor& c,PoliticsGovernanceState& s){return state_fields(c,s,3);}
Status emit_politics_governance3_projection(ProjectionWriter& sink,std::string_view name,const PoliticsGovernanceState& s){return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,s,[](auto& c,auto& v){return state_fields(c,v,3);});}
Outcome<BoundaryValue> encode_politics_governance3(const PoliticsGovernanceState& s){BoundaryValue out;BoundaryProjectionWriter sink(out);auto valid=emit_politics_governance3_projection(sink,"",s);if(valid.ok)valid=sink.finish();return {valid.ok,std::move(out),valid.error};}
Outcome<PoliticsGovernanceState> decode_politics_governance3(const BoundaryValue& value){PoliticsGovernanceState out;ActorFoundationJsonVisitor reader(value);auto valid=state_fields(reader,out,3);if(valid.ok)valid=reader.finish();return {valid.ok,std::move(out),valid.error};}

}
