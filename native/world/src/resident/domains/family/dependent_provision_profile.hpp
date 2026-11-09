// P18.9 original infant requirements. This is a locked catalog definition,
// never a paid fare, a capacity allowance, or an applied provision binding.
#pragma once
#include "born_character_source.hpp"
namespace hanmo::resident {
struct DependentProvisionProfile {
 StableId profile_id,version,age_stage="infant";
 StableId food_requirement_profile_ref,care_requirement_profile_ref,transport_cost_policy_ref;
 Integer infant_weight_units=0;StableId weight_unit_id;
};
struct DependentProvisionCatalog {
 CatalogIdentity identity;StableId birth_profile_id;
 OrderedTable<DependentProvisionProfile> profiles;
};
Status validate_dependent_provision_profile_shape(const DependentProvisionProfile&);
Status validate_dependent_provision_catalog(const DependentProvisionCatalog&);
// Only the selected Birth4 profile is installed. A definition alone does not
// grant any transport/provision permission; those original owner readers must
// resolve its three references and the Common weight unit at composition/use.
Outcome<DependentProvisionCatalog> decode_dependent_provision_catalog(
 const BoundaryValue& definitions,const StableId& selected_profile_id,const CatalogIdentity&);
struct BirthDependentProvisionProfileView {
 const DependentProvisionProfile* profile=nullptr;
 const BirthReply* original_acceptance=nullptr;
};
Outcome<BirthDependentProvisionProfileView> read_dependent_provision_profile_at_due(
 const BornCharacterSourceContext&,const DependentProvisionCatalog&,
 const StableId& delivery_actor_id,const StableId& accepted_id,
 const ReadStamp&,const ReadStampAuthority&,const DomainReadVersionSource&);
Outcome<BirthDependentProvisionProfileView> read_dependent_provision_profile_at_birth(
 const BornCharacterSourceContext&,const DependentProvisionCatalog&,const BirthFactRef&);
class ProjectionWriter;
Status visit_dependent_provision_profile(CodecVisitor&,DependentProvisionProfile&);
Status emit_dependent_provision_profile_fields(ProjectionWriter&,const DependentProvisionProfile&);
Outcome<DependentProvisionProfile> decode_dependent_provision_profile(const BoundaryValue&);
namespace dependent_provision_codec {
template<class Archive,class Value>Status fields(Archive& c,Value& value){
 c.field("profile_id",value.profile_id);c.field("version",value.version);
 c.field("age_stage",value.age_stage);c.field("food_requirement_profile_ref",value.food_requirement_profile_ref);
 c.field("care_requirement_profile_ref",value.care_requirement_profile_ref);
 c.field("transport_cost_policy_ref",value.transport_cost_policy_ref);
 c.field("infant_weight_units",value.infant_weight_units);c.field("weight_unit_id",value.weight_unit_id);
 if(!c.status().ok)return c.status();auto valid=validate_dependent_provision_profile_shape(value);
 return valid.ok?c.status():c.reject(valid.error.original.code);
}
}
}
