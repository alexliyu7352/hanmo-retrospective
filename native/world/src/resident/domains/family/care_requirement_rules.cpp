#include "resident/kernel/catalog_section_observation.hpp"
#include "care_requirement_rules.hpp"
#include "resident/domains/actor_foundation/authored_origin.hpp"
namespace hanmo::resident {
Status validate_care_requirement_catalog(const CareRequirementCatalog& c){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 26, "CareRequirementCatalog", "validate_care_requirement_catalog", &c);

 if(c.selected_version<3||c.selected_version>6||c.identity.content_digest.empty()||c.birth_requirement_rule_id.empty()||!c.rules.find(c.birth_requirement_rule_id))return common_failure("CareSelectedCatalogRequired",{},ErrorKind::unsupported_capability);
 bool module=false;for(const auto& entry:c.identity.module_versions)if(entry.first=="care_requirements"){if(module||entry.second!=std::to_string(c.selected_version))return common_failure("CareCatalogVersionMismatch",{},ErrorKind::internal);module=true;}if(!module)return common_failure("CareCatalogVersionRequired",{},ErrorKind::unsupported_capability);
 for(const auto& row:c.rules.rows()){
  const auto& rule=row.second;if(row.first!=rule.rule_ref.id||rule.service_kind!="care_handover"||rule.state_need_policy_ref.empty())return common_failure("InvalidCareRequirementRule",row.first,ErrorKind::internal);
  for(const auto* ref:{&rule.rule_ref,&rule.dependency_rule_ref,&rule.cancellation_rule_ref,&rule.disclosure_rule_ref})if(ref->id.empty()||ref->version<1||ref->catalog_digest!=c.identity.content_digest)return common_failure("CareRuleCatalogMismatch",row.first,ErrorKind::internal);
 }
 return common_success();
}
Outcome<const CareRequirementRule*> NativeCareRequirementRuleRead::read_care_requirement_rule(const ActorFoundationRuleRef& ref)const{
 auto valid=validate_care_requirement_catalog(catalog_);if(!valid.ok)return {false,nullptr,valid.error};const auto* row=catalog_.rules.find(ref.id);if(!row||!actor_foundation_rule_equal(row->rule_ref,ref))return {false,nullptr,common_failure("CareSelectedRuleMissing",ref.id,ErrorKind::unsupported_capability).error};return {true,row,{}};
}
Outcome<const CareRequirementRule*> NativeCareRequirementRuleRead::read_birth_care_rule(const BornCharacterSourceView& birth)const{
 auto valid=validate_care_requirement_catalog(catalog_);if(!valid.ok)return {false,nullptr,valid.error};if(!births_.bound_to(common_))return {false,nullptr,common_failure("CareBirthSameCandidateRequired",{},ErrorKind::internal).error};if(!birth.birth||!birth.accepted||!birth.original_fact||birth.original_fact!=common_.state().facts.find(birth.original_fact->id)||birth.original_fact->domain!="family_birth"||birth.original_fact->kind!="family_birth_recorded"||birth.birth->kind!="family_birth_recorded"||birth.accepted->kind!="family_birth_accepted"||birth.accepted->decision!="accepted"||birth.birth->actor_id.empty()||!common_.actor(birth.birth->actor_id))return {false,nullptr,common_failure("CareBirthOriginalSourceRequired",{},ErrorKind::internal).error};auto actual=births_.read_original_birth({birth.original_fact->id});if(!actual.ok)return {false,nullptr,actual.error};auto same_at=compare_world_moments(actual.value.occurred_at,birth.occurred_at);if(!same_at.ok)return {false,nullptr,same_at.error};if(actual.value.birth!=birth.birth||actual.value.accepted!=birth.accepted||actual.value.original_fact!=birth.original_fact||!same_at.ok||same_at.value!=0)return {false,nullptr,common_failure("CareBirthOriginalSourceMismatch",birth.original_fact->id,ErrorKind::internal).error};return {true,catalog_.rules.find(catalog_.birth_requirement_rule_id),{}};
}
}
