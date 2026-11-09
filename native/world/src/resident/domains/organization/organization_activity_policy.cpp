#include "organization_activity_policy.hpp"
namespace hanmo::resident {namespace {
bool rule(const OrganizationActivityPolicyRuleRef& r){return !r.id.empty()&&r.version>0&&!r.catalog_digest.empty();}
bool identifiers(const std::vector<StableId>& values){
 for(std::size_t i=0;i<values.size();++i){if(values[i].empty())return false;for(std::size_t j=0;j<i;++j)if(values[j]==values[i])return false;}return true;
}
}
Status validate_organization_activity_policy_shape(const OrganizationActivityPolicy& p){
 if(p.id.empty()||p.version!=1||p.organization_kind.empty()||!identifiers(p.allowed_template_keys)||
    !identifiers(p.allowed_activity_kinds)||!rule(p.authority_policy_ref)||!rule(p.reserve_policy_ref)||!rule(p.report_policy_ref)||
    p.max_candidates_per_review<1||p.max_optional_adoptions_per_boundary<1||
    p.retry_delay_ticks<1||p.known_report_freshness_ticks<1)return common_failure("InvalidOrganizationActivityPolicy",p.id);
 for(std::size_t i=0;i<p.allowed_scope_keys.size();++i){const auto& s=p.allowed_scope_keys[i];
  if(s.id.empty()||static_cast<Integer>(s.kind)<0||static_cast<Integer>(s.kind)>static_cast<Integer>(ScopeRef::Kind::activity))return common_failure("InvalidOrganizationActivityPolicyScope",p.id);
  for(std::size_t j=0;j<i;++j)if(s.kind==p.allowed_scope_keys[j].kind&&s.id==p.allowed_scope_keys[j].id)return common_failure("DuplicateOrganizationActivityPolicyScope",p.id);
 }
 return common_success();
}
Status validate_organization_activity_policy(const OrganizationActivityPolicy& p,const CatalogIdentity& identity,
 const OrganizationActivityPolicyOriginalRead& original,const OrganizationActivityPolicyLimitRead& technical){
 auto valid=validate_organization_activity_policy_shape(p);if(!valid.ok)return valid;
 if(identity.content_digest.empty()||p.authority_policy_ref.catalog_digest!=identity.content_digest||
    p.reserve_policy_ref.catalog_digest!=identity.content_digest||p.report_policy_ref.catalog_digest!=identity.content_digest)return common_failure("OrganizationActivityPolicyCatalogMismatch",p.id);
 auto limits=technical.read_organization_activity_policy_limits(identity);if(!limits.ok)return {false,{},limits.error};const auto& l=limits.value;
 if(l.trusted_profile_id.empty()||l.trusted_profile_revision<1||l.max_candidates_per_review<1||l.max_optional_adoptions_per_boundary<1||
    l.maximum_retry_delay_ticks<1||l.maximum_known_report_freshness_ticks<1)return common_failure("OrganizationActivityPolicyTrustedLimitsRequired",p.id);
 if(p.max_candidates_per_review>l.max_candidates_per_review||p.max_optional_adoptions_per_boundary>l.max_optional_adoptions_per_boundary||
    p.retry_delay_ticks>l.maximum_retry_delay_ticks||p.known_report_freshness_ticks>l.maximum_known_report_freshness_ticks)return common_failure("OrganizationActivityPolicyLimitExceeded",p.id,ErrorKind::unsupported_capability);
 valid=original.validate_organization_activity_policy_kind(p.organization_kind);if(!valid.ok)return valid;
 for(const auto& key:p.allowed_template_keys){valid=original.validate_organization_activity_template(p.organization_kind,key);if(!valid.ok)return valid;}
 for(const auto& key:p.allowed_activity_kinds){valid=original.validate_organization_activity_kind(p.organization_kind,key);if(!valid.ok)return valid;}
 for(const auto& key:p.allowed_scope_keys){valid=original.validate_organization_activity_scope(key);if(!valid.ok)return valid;}
 valid=original.validate_organization_activity_authority_policy(p.authority_policy_ref);if(!valid.ok)return valid;
 valid=original.validate_organization_activity_reserve_policy(p.reserve_policy_ref);if(!valid.ok)return valid;
 return original.validate_organization_activity_report_policy(p.report_policy_ref);
}
Outcome<const OrganizationActivityPolicy*> read_organization_activity_policy(const OrganizationRecord& organization,
 const OrderedTable<OrganizationActivityPolicy>& policies,const CatalogIdentity& identity,
 const OrganizationActivityPolicyOriginalRead& original,const OrganizationActivityPolicyLimitRead& technical){
 const auto* p=policies.find(organization.policy_profile_id);
 if(organization.id.empty()||organization.kind.empty()||!p||p->id!=organization.policy_profile_id||p->organization_kind!=organization.kind)
  return {false,nullptr,common_failure("OrganizationActivityPolicyBindingMismatch",organization.id).error};
 auto valid=validate_organization_activity_policy(*p,identity,original,technical);return {valid.ok,valid.ok?p:nullptr,valid.error};
}
}
