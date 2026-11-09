// N2.1正文逐项13字段；三出口同表，作者策略不增加World根/隐含登记或权限。
#pragma once
#include "organization_activity_policy.hpp"
#include "organization_lifecycle_codec_detail.hpp"
namespace hanmo::resident::organization_activity_policy_fields {
template<class C>Status scope(C& c,ScopeRef& value){static constexpr const char* names[]={"site","region","force","account","activity"};organization_lifecycle_fields::enum_field(c,"kind",value.kind,names);return c.field("id",value.id);}
template<class C>Status fields(C& c,OrganizationActivityPolicy& p){
 c.field("id",p.id);c.field("version",p.version);c.field("organization_kind",p.organization_kind);
 organization_lifecycle_fields::strings(c,"allowed_template_keys",p.allowed_template_keys);
 organization_lifecycle_fields::strings(c,"allowed_activity_kinds",p.allowed_activity_kinds);
 c.object("authority_policy_ref",[&](auto& v){return organization_lifecycle_fields::rule(v,p.authority_policy_ref);});
 c.sequence("allowed_scope_keys",p.allowed_scope_keys,[](auto& v,auto& s){return scope(v,s);});
 c.object("reserve_policy_ref",[&](auto& v){return organization_lifecycle_fields::rule(v,p.reserve_policy_ref);});
 c.object("report_policy_ref",[&](auto& v){return organization_lifecycle_fields::rule(v,p.report_policy_ref);});
 c.field("max_candidates_per_review",p.max_candidates_per_review);c.field("max_optional_adoptions_per_boundary",p.max_optional_adoptions_per_boundary);
 c.field("retry_delay_ticks",p.retry_delay_ticks);c.field("known_report_freshness_ticks",p.known_report_freshness_ticks);
 if(c.status().ok){auto valid=validate_organization_activity_policy_shape(p);if(!valid.ok)return c.reject(valid.error.original.code,valid.error.original.path);}return c.status();
}
}
