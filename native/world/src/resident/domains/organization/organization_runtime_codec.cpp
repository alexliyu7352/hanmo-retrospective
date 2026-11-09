// M5完整十二根共享唯一字段；JSON表和二进制表保原有序键，不给缺根默认空。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "organization_runtime.hpp"
#include "organization_lifecycle_codec_detail.hpp"
#include "organization_authority_codec_fields.hpp"
#include "authority_basis_codec_fields.hpp"
namespace hanmo::resident {namespace {
using namespace organization_lifecycle_fields;
template<class C>Status office(C& c,VerifiedOfficeTransition& r){c.object("source",[&](auto& x){return source(x,r.source);});c.field("organization_id",r.organization_id);c.field("office_id",r.office_id);c.field("expected_epoch",r.expected_epoch);c.optional("old_holder",r.old_holder,[](auto& x,auto& s){return x.field("value",s);});c.optional("new_holder",r.new_holder,[](auto& x,auto& s){return x.field("value",s);});{static constexpr const char* tags[]={"install","dismiss","resign","term_handover","accession"};enum_field(c,"kind",r.kind,tags);}return c.object("effective_moment",[&](auto& x){return exact_fields::moment(x,r.effective_moment);});}
template<class C>Status version(C& c,OrganizationGrantVersionRef& r){c.field("id",r.id);c.field("grant_id",r.grant_id);c.field("revision",r.revision);return c.object("change_source",[&](auto& x){return source(x,r.change_source);});}
template<class C>Status relation(C& c,OrganizationRelationBinding& r){c.field("id",r.id);c.field("organization_id",r.organization_id);c.field("related_organization_id",r.related_organization_id);{static constexpr const char* tags[]={"administrative_under","branch_of","political_submission","alliance","protection","military_command_binding"};enum_field(c,"kind",r.kind,tags);}return c.object("source_ref",[&](auto& x){return source(x,r.source_ref);});}
template<class C>Status fields(C& c,OrderedTable<GrantRuntimeRecord>& grants,OrganizationRuntime& s,bool named_membership){
 Integer schema=1;c.field("schema_version",schema);if(!c.status().ok)return c.status();if(schema!=1)return c.reject("UnsupportedOrganizationRuntimeSchema");
 table(c,"organization_metadata",s.organization_metadata,[](auto& v,auto& r){return metadata(v,r);});
 table(c,"lifecycle_origins",s.lifecycle_origins,[](auto& v,auto& r){return origin(v,r);});
 table(c,"lifecycle_migration_decisions",s.lifecycle_migration_decisions,[](auto& v,auto& r){return migration(v,r);});
 table(c,"lifecycle_transitions",s.lifecycle_transitions,[](auto& v,auto& r){return transition(v,r);});
 table(c,"office_transition_sources",s.office_transition_sources,[](auto& v,auto& r){return office(v,r);});
 table(c,"grants",grants,[&](auto& v,auto& r){return organization_authority_codec_fields::record_value(v,r,named_membership);});
 table(c,"grant_versions",s.grant_versions,[](auto& v,auto& r){return version(v,r);});
 table(c,"authority_bases",s.authority_bases,[](auto& v,auto& r){return authority_basis_codec_fields::basis(v,r);});
 table(c,"relations",s.relations,[](auto& v,auto& r){return relation(v,r);});
 table(c,"lifecycle_outcomes",s.lifecycle_outcomes,[](auto& v,auto& r){return outcome(v,r);});
 return table(c,"terminal_dispositions",s.terminal_dispositions,[](auto& v,auto& r){return disposition(v,r);});
}
}
Status visit_organization_runtime(CodecVisitor& c,OrganizationState& org,OrganizationRuntime& s,bool named){if(c.reading()&&!org.current_grants.empty())return c.reject("OrganizationRuntimeGrantOwnerConflict");return fields(c,org.current_grants,s,named);}
Status emit_organization_runtime_projection(ProjectionWriter& sink,std::string_view name,const OrganizationState& org,const OrganizationRuntime& s,bool named){return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,s,[&](auto& c,auto& value){return fields(c,const_cast<OrderedTable<GrantRuntimeRecord>&>(org.current_grants),value,named);});}
Outcome<BoundaryValue> encode_organization_runtime(const OrganizationState& org,const OrganizationRuntime& s,bool named){BoundaryValue out;BoundaryProjectionWriter sink(out);auto valid=emit_organization_runtime_projection(sink,"",org,s,named);if(valid.ok)valid=sink.finish();return {valid.ok,std::move(out),valid.error};}
Outcome<OrganizationRuntime> decode_organization_runtime(const BoundaryValue& input,OrganizationState& candidate,bool named){
 // A runtime-selected body must not have decoded a second grants field. Do not
 // silently overwrite it, even when both physical copies happen to agree.
 if(!candidate.current_grants.empty())return {false,{},common_failure("OrganizationRuntimeGrantOwnerConflict").error};
 ActorFoundationJsonVisitor reader(input);auto valid=reader.prepare_default_values<OrganizationRuntime>();
 if(!valid.ok)return {false,{},valid.error};OrganizationRuntime out;
 valid=reader.prepare_default_values<OrderedTable<GrantRuntimeRecord>>();if(!valid.ok)return {false,{},valid.error};OrderedTable<GrantRuntimeRecord> grants;
 valid=fields(reader,grants,out,named);if(valid.ok)valid=reader.finish();
 if(valid.ok)candidate.current_grants=std::move(grants);
 return {valid.ok,std::move(out),valid.error};
}
}
