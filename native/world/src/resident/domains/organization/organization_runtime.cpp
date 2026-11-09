#include "organization_runtime.hpp"
#include "grant_authority.hpp"
#include <set>
namespace hanmo::resident {
Outcome<OrganizationRuntimeCodecSelection> select_organization_runtime_codec(const CatalogIdentity& identity){
 const StableId* body=nullptr;const StableId* runtime=nullptr;const StableId* effects=nullptr;
 for(const auto& module:identity.module_versions){
  if(module.first=="organization"){if(body)return {false,{},common_failure("DuplicateOrganizationPhysicalModule").error};body=&module.second;}
  if(module.first=="organization_runtime"){if(runtime)return {false,{},common_failure("DuplicateOrganizationRuntimeModule").error};runtime=&module.second;}
  if(module.first=="organization_service_effects"){if(effects)return {false,{},common_failure("DuplicateOrganizationServiceEffectsModule").error};effects=&module.second;}
 }
 if(!runtime||*runtime!="1")return {false,{},common_failure("OrganizationRuntimeModule1Required").error};
 if(!body||(*body!="4"&&*body!="5"&&*body!="6"&&*body!="7"))return {false,{},common_failure("OrganizationRuntimePhysicalBodyRequired").error};
 const Integer version=*body=="4"?4:*body=="5"?5:*body=="6"?6:7;
 if(effects&&(*effects!="1"||version<6))return {false,{},common_failure("OrganizationServiceEffectsExactBodyRequired").error};
 return {true,{version,version==5||version==7,version>=6,effects?1:0},{}};
}
Status validate_organization_runtime_codec_selection(const CatalogIdentity& identity,const OrganizationCatalog& catalog){
 auto selected=select_organization_runtime_codec(identity);if(!selected.ok)return {false,{},selected.error};
 const auto& locked=catalog.authority_catalog_identity;
 if(!catalog.enabled||!catalog.authority_lifecycle_enabled||catalog.named_membership_enabled!=selected.value.named_membership||catalog.exact_fact_clock_enabled!=selected.value.exact_fact_clock||catalog.service_effects_enabled!=(selected.value.service_effects_schema==1)||locked.content_id!=identity.content_id||locked.content_version!=identity.content_version||locked.artifact_sha256!=identity.artifact_sha256||locked.content_digest!=identity.content_digest||locked.ruleset_id!=identity.ruleset_id||locked.schema_sha256!=identity.schema_sha256||locked.pack_set_digest!=identity.pack_set_digest||locked.module_versions!=identity.module_versions)return common_failure("OrganizationRuntimeCatalogSelectionMismatch");
 return common_success();
}
Status index_original_grant_versions_in_candidate(const CommonStore& common,const OrganizationState& org,OrganizationRuntime& runtime){
 for(const auto& id:org.facts.order()){const auto* fact=org.facts.find(id);if(!fact)return common_failure("OrganizationFactOrderMismatch",id);if(!fact->grant_change_payload)continue;const auto* header=common.state().facts.find(id);const auto& change=*fact->grant_change_payload;if(!header||header->domain!="organization"||header->kind!=fact->kind||change.after_revision!=change.after.revision||change.grant_id!=change.after.terms.id)return common_failure("InvalidOriginalGrantVersion",id);
  OrganizationGrantVersionRef row{id,change.grant_id,change.after_revision,{"organization",fact->kind,id}};if(const auto* old=runtime.grant_versions.find(id)){if(old->id!=row.id||old->grant_id!=row.grant_id||old->revision!=row.revision||!same_source_ref(old->change_source,row.change_source))return common_failure("GrantVersionIndexMismatch",id);}else if(!runtime.grant_versions.insert(id,std::move(row)))return common_failure("DuplicateGrantVersionIndex",id);
 }
 return common_success();
}
Status validate_organization_runtime_sources(const CommonStore& common,const OrganizationState& org,const OrganizationRuntime& runtime,const OrganizationCatalog& catalog,const OrganizationLifecycleCatalog& lifecycle,const CatalogIdentity& identity,const ReadStamp& stamp,const OrganizationLifecycleSourceRead& sources,const OrganizationRelationSourceRead& relations,const OfficeTransitionSourceRead& offices,const VerifiedDeathRead& deaths,const AuthorityIssueSourceRead& issues,const AuthorityBudgetRead* budgets,const WorldMoment& now){
 auto valid=validate_organization_lifecycle(common,org,runtime,catalog,lifecycle,stamp,sources,now);if(!valid.ok)return valid;valid=validate_grant_authority_history(common,org,catalog,&issues);if(!valid.ok)return valid;valid=validate_authority_basis_registry(common,runtime.authority_bases,org,catalog,identity,issues,budgets);if(!valid.ok)return valid;valid=validate_office_lifecycle(common,org,catalog,stamp,offices,deaths);if(!valid.ok)return valid;
 std::set<StableId> versions,transition_sources;
 for(const auto& item:org.facts.rows()){
  const auto& fact=item.second;if(fact.grant_change_payload){const auto& p=*fact.grant_change_payload;const auto* v=runtime.grant_versions.find(item.first);if(!v||v->id!=item.first||v->grant_id!=p.grant_id||v->revision!=p.after_revision||!same_source_ref(v->change_source,{"organization",fact.kind,item.first})||!versions.insert(item.first).second)return common_failure("GrantVersionIndexCoverage",item.first);}
  if(fact.office_change_payload)if(const auto* cause=std::get_if<ByOfficeTransition>(&fact.office_change_payload->cause)){
   const auto* stored=runtime.office_transition_sources.find(cause->transition_source_ref.id);if(!stored||!same_source_ref(stored->source,cause->transition_source_ref)||!transition_sources.insert(stored->source.id).second)return common_failure("OfficeTransitionSourceCoverage",item.first);
   auto actual=offices.read_office_transition(stored->source,stamp);if(!actual.ok)return {false,{},actual.error};auto m=compare_world_moments(actual.value.effective_moment,stored->effective_moment);if(!m.ok)return {false,{},m.error};if(actual.value.organization_id!=stored->organization_id||actual.value.office_id!=stored->office_id||actual.value.expected_epoch!=stored->expected_epoch||actual.value.old_holder!=stored->old_holder||actual.value.new_holder!=stored->new_holder||actual.value.kind!=stored->kind||m.value!=0)return common_failure("OfficeTransitionSourceMismatch",stored->source.id);
  }
 }
 if(versions.size()!=runtime.grant_versions.rows().size()||transition_sources.size()!=runtime.office_transition_sources.rows().size())return common_failure("OrphanOrganizationAuthoritySource");
 for(const auto& item:runtime.relations.rows()){if(item.first!=item.second.id||!org.organizations.find(item.second.organization_id)||!org.organizations.find(item.second.related_organization_id)||item.second.organization_id==item.second.related_organization_id)return common_failure("InvalidOrganizationRelation",item.first);valid=relations.validate_organization_relation(item.second,stamp);if(!valid.ok)return valid;}
 return common_success();
}
}
