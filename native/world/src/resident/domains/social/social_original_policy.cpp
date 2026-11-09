#include "social_original_policy.hpp"
#include "social_model.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
OriginalSocialPolicyLookup lookup_original_social_policy(const CommonStore& c,
 const SocialCatalog& catalog,const StableId& id){
 // Preserve the original policy_for precedence, including a direct authored
 // key before the optional dynamic actor prototype lookup.
 for(const auto& row:catalog.policies.rows())if(row.first==id)
  return {&row.second,&row.first,SocialPolicySource::Lookup::actor_id};
 const auto* actor=c.state().actors.find(id);
 if(actor&&actor->prototype_id)for(const auto& row:catalog.policies.rows())
  if(row.first==*actor->prototype_id)
   return {&row.second,&row.first,SocialPolicySource::Lookup::prototype_id};
 return {};
}
namespace {
bool same(const CatalogIdentity& a,const CatalogIdentity& b){
 return a.content_id==b.content_id&&a.content_version==b.content_version&&
  a.content_digest==b.content_digest&&a.ruleset_id==b.ruleset_id&&
  a.schema_sha256==b.schema_sha256&&a.pack_set_digest==b.pack_set_digest&&
  a.module_versions==b.module_versions&&a.artifact_sha256==b.artifact_sha256;
}
Status context(const CommonStore& c,const CatalogIdentity& catalog,const StableId& actor){
 if(!c.actor(actor))return common_failure("SocialPolicyActorMissing",actor,ErrorKind::internal);
 if(catalog.content_digest.empty()||catalog.artifact_sha256.empty())
  return common_failure("SocialPolicyOriginalCatalogRequired",actor,ErrorKind::unsupported_capability);
 return common_success();
}
Status shape(const OriginalSocialPolicyLookup& found){
 if(!found.policy)return common_success();
 if(!found.original_catalog_key||found.original_catalog_key->empty()||
  (found.policy->kind!="cooperative"&&found.policy->kind!="selfish")||
  found.policy->interval_ticks<1||found.policy->interval_ticks>120)
  return common_failure("InvalidSocialPolicy",{},ErrorKind::internal);
 return common_success();
}
}
Outcome<SocialDispositionPolicyBasis> read_original_social_policy_basis(
 const CommonStore& c,const SocialCatalog& catalog,const CatalogIdentity& identity,
 const StableId& actor){
 auto valid=context(c,identity,actor);if(!valid.ok)return {false,{},valid.error};
 const auto found=lookup_original_social_policy(c,catalog,actor);
 valid=shape(found);if(!valid.ok)return {false,{},valid.error};
 if(!found.policy)return {true,UnknownSocialPolicyBasis{}, {}};
 KnownSocialPolicyBasis value;
 value.policy_source={identity,found.lookup,*found.original_catalog_key};
 value.kind=found.policy->kind=="cooperative"?KnownSocialPolicyBasis::Kind::cooperative:KnownSocialPolicyBasis::Kind::selfish;
 value.share_news=found.policy->share_news;value.interval_ticks=found.policy->interval_ticks;
 return {true,std::move(value),{}};
}
Status validate_original_social_policy_basis(const CommonStore& c,
 const SocialCatalog& catalog,const CatalogIdentity& identity,const StableId& actor,
 const SocialDispositionPolicyBasis& value){
 auto valid=context(c,identity,actor);if(!valid.ok)return valid;
 const auto found=lookup_original_social_policy(c,catalog,actor);
 valid=shape(found);if(!valid.ok)return valid;
 if(!found.policy){const auto* unknown=std::get_if<UnknownSocialPolicyBasis>(&value);
  return unknown&&unknown->reason==UnknownSocialPolicyBasis::Reason::no_matching_authored_policy?
   common_success():common_failure("SocialPolicyUnknownBasisMismatch",actor,ErrorKind::internal);
 }
 const auto* known=std::get_if<KnownSocialPolicyBasis>(&value);
 if(!known||!same(known->policy_source.catalog_identity,identity)||
  known->policy_source.lookup!=found.lookup||
  known->policy_source.policy_actor_key!=*found.original_catalog_key||
  known->kind!=(found.policy->kind=="cooperative"?KnownSocialPolicyBasis::Kind::cooperative:KnownSocialPolicyBasis::Kind::selfish)||
  known->share_news!=found.policy->share_news||known->interval_ticks!=found.policy->interval_ticks)
  return common_failure("SocialPolicyOriginalBasisMismatch",actor,ErrorKind::internal);
 return common_success();
}
}
