#include "resident/kernel/catalog_section_observation.hpp"
#include "dependent_provision_profile.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
namespace hanmo::resident {namespace {
bool identifier(const StableId& value){return !value.empty()&&value.size()<=256;}
Status same_catalog(const BornCharacterSourceContext& x,const DependentProvisionCatalog& catalog){
 auto valid=validate_dependent_provision_catalog(catalog);if(!valid.ok)return valid;
 if(x.catalog.selected_version!=4||!x.catalog.enabled||!x.catalog.dependent_provision_profile_ref||*x.catalog.dependent_provision_profile_ref!=catalog.birth_profile_id||catalog.identity.content_digest!=x.common.state().metadata.content_digest||!x.catalog.due_policy_ref||catalog.identity.content_digest!=x.catalog.due_policy_ref->catalog_digest)return common_failure("DependentProvisionBirthCatalogMismatch",{},ErrorKind::internal);
 return common_success();
}
}
Status validate_dependent_provision_profile_shape(const DependentProvisionProfile& value){
 for(const auto* id:{&value.profile_id,&value.version,&value.food_requirement_profile_ref,&value.care_requirement_profile_ref,&value.transport_cost_policy_ref,&value.weight_unit_id})if(!identifier(*id))return common_failure("InvalidDependentProvisionProfile",value.profile_id,ErrorKind::internal);
 if(value.age_stage!="infant"||value.infant_weight_units<=0)return common_failure("InvalidInfantProvisionWeight",value.profile_id,ErrorKind::internal);
 return common_success();
}
Status validate_dependent_provision_catalog(const DependentProvisionCatalog& catalog){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 31, "DependentProvisionCatalog", "validate_dependent_provision_catalog", &catalog);

 if(catalog.identity.content_digest.empty()||!identifier(catalog.birth_profile_id)||catalog.profiles.size()!=1||!catalog.profiles.find(catalog.birth_profile_id))return common_failure("SelectedDependentProvisionProfileRequired",{},ErrorKind::unsupported_capability);
 bool birth=false;for(const auto& module:catalog.identity.module_versions)if(module.first=="family_birth"){if(birth||module.second!="4")return common_failure("DependentProvisionBirth4Required",{},ErrorKind::unsupported_capability);birth=true;}if(!birth)return common_failure("DependentProvisionBirth4Required",{},ErrorKind::unsupported_capability);
 for(const auto& row:catalog.profiles.rows()){if(row.first!=row.second.profile_id)return common_failure("DependentProvisionProfileKeyMismatch",row.first,ErrorKind::internal);auto valid=validate_dependent_provision_profile_shape(row.second);if(!valid.ok)return valid;}
 return common_success();
}
Status visit_dependent_provision_profile(CodecVisitor& c,DependentProvisionProfile& value){return dependent_provision_codec::fields(c,value);}
Status emit_dependent_provision_profile_fields(ProjectionWriter& w,const DependentProvisionProfile& value){ActorFoundationProjectionArchive c(w);return dependent_provision_codec::fields(c,const_cast<DependentProvisionProfile&>(value));}
Outcome<DependentProvisionProfile> decode_dependent_provision_profile(const BoundaryValue& input){DependentProvisionProfile value;ActorFoundationJsonVisitor c(input);auto valid=dependent_provision_codec::fields(c,value);if(valid.ok)valid=c.finish();return {valid.ok,std::move(value),valid.error};}
Outcome<DependentProvisionCatalog> decode_dependent_provision_catalog(const BoundaryValue& definitions,const StableId& selection,const CatalogIdentity& identity){try{
 using namespace boundary;const auto& rows=object(definitions,"dependent_provision_profiles");if(!identifier(selection)||rows.empty()||rows.size()>4096)fail("InvalidDependentProvisionSelection",selection);
 for(std::size_t i=0;i<rows.size();++i){if(!identifier(rows[i].first))fail("InvalidDependentProvisionProfile",rows[i].first);for(std::size_t j=0;j<i;++j)if(rows[j].first==rows[i].first)fail("DuplicateDependentProvisionProfile",rows[i].first);}
 auto value=decode_dependent_provision_profile(field(definitions,selection,"dependent_provision_profiles"));if(!value.ok)return {false,{},value.error};if(value.value.profile_id!=selection)fail("DependentProvisionProfileKeyMismatch",selection);
 DependentProvisionCatalog catalog{identity,selection};if(!catalog.profiles.insert(selection,std::move(value.value)))fail("DuplicateDependentProvisionProfile",selection);auto valid=validate_dependent_provision_catalog(catalog);return {valid.ok,std::move(catalog),valid.error};
 }catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
Outcome<BirthDependentProvisionProfileView> read_dependent_provision_profile_at_due(const BornCharacterSourceContext& x,const DependentProvisionCatalog& catalog,const StableId& delivery,const StableId& accepted,const ReadStamp& stamp,const ReadStampAuthority& stamps,const DomainReadVersionSource& versions){
 auto valid=same_catalog(x,catalog);if(!valid.ok)return {false,{},valid.error};auto actual=read_birth_placement_arrangement_at_due(x,delivery,accepted,stamp,stamps,versions);if(!actual.ok)return {false,{},actual.error};return {true,{catalog.profiles.find(catalog.birth_profile_id),actual.value.accepted},{}};
}
Outcome<BirthDependentProvisionProfileView> read_dependent_provision_profile_at_birth(const BornCharacterSourceContext& x,const DependentProvisionCatalog& catalog,const BirthFactRef& birth){
 auto valid=same_catalog(x,catalog);if(!valid.ok)return {false,{},valid.error};auto actual=read_born_character_source(x,birth);if(!actual.ok)return {false,{},actual.error};return {true,{catalog.profiles.find(catalog.birth_profile_id),actual.value.accepted},{}};
}
}
