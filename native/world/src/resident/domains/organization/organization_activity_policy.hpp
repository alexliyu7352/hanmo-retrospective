// 固定M-N2.1原目录行；只约束有限提议，不签发权源、接受义务或复制Director计划。
#pragma once
#include "organization_model.hpp"
#include <string_view>
namespace hanmo::resident {
class ProjectionWriter;
// 本域原策略目录身份；字段同形不表示已注册ActorFoundation规则或取得任何公权。
struct OrganizationActivityPolicyRuleRef {StableId id;Integer version=0;StableId catalog_digest;};
struct OrganizationActivityPolicy {
 StableId id;Integer version=1;StableId organization_kind;
 std::vector<StableId> allowed_template_keys,allowed_activity_kinds;
 OrganizationActivityPolicyRuleRef authority_policy_ref;
 std::vector<ScopeRef> allowed_scope_keys;
 OrganizationActivityPolicyRuleRef reserve_policy_ref,report_policy_ref;
 Integer max_candidates_per_review=0,max_optional_adoptions_per_boundary=0;
 Integer retry_delay_ticks=0,known_report_freshness_ticks=0;
};
// 只有原受信配置owner可提供，文件/目录不解码此值。当前Checkpoint CapacityProfile
// 没有这四个业务上限，不能把其collection/bytes数拿来默认填充。
struct OrganizationActivityPolicyLimits {
 StableId trusted_profile_id;Integer trusted_profile_revision=0;
 Integer max_candidates_per_review=0,max_optional_adoptions_per_boundary=0;
 Integer maximum_retry_delay_ticks=0,maximum_known_report_freshness_ticks=0;
};
class OrganizationActivityPolicyLimitRead {public:virtual ~OrganizationActivityPolicyLimitRead()=default;
 virtual Outcome<OrganizationActivityPolicyLimits> read_organization_activity_policy_limits(const CatalogIdentity&)const=0;
};
// 读真实锁内定义和已编译原模板/活动注册；任何未知引用必须拒，不能以字符串存在自授。
class OrganizationActivityPolicyOriginalRead {public:virtual ~OrganizationActivityPolicyOriginalRead()=default;
 virtual Status validate_organization_activity_policy_kind(const StableId&)const=0;
 virtual Status validate_organization_activity_template(const StableId& organization_kind,const StableId& template_key)const=0;
 virtual Status validate_organization_activity_kind(const StableId& organization_kind,const StableId& activity_kind)const=0;
 virtual Status validate_organization_activity_scope(const ScopeRef&)const=0;
 virtual Status validate_organization_activity_authority_policy(const OrganizationActivityPolicyRuleRef&)const=0;
 virtual Status validate_organization_activity_reserve_policy(const OrganizationActivityPolicyRuleRef&)const=0;
 virtual Status validate_organization_activity_report_policy(const OrganizationActivityPolicyRuleRef&)const=0;
};
Status validate_organization_activity_policy_shape(const OrganizationActivityPolicy&);
Status validate_organization_activity_policy(const OrganizationActivityPolicy&,const CatalogIdentity&,
 const OrganizationActivityPolicyOriginalRead&,const OrganizationActivityPolicyLimitRead&);
// 原policy_profile_id只解析同锁这一行；通过此读不等于机构已知、批准或具备真实执行者。
Outcome<const OrganizationActivityPolicy*> read_organization_activity_policy(
 const OrganizationRecord&,const OrderedTable<OrganizationActivityPolicy>&,const CatalogIdentity&,
 const OrganizationActivityPolicyOriginalRead&,const OrganizationActivityPolicyLimitRead&);
Status visit_organization_activity_policy(CodecVisitor&,OrganizationActivityPolicy&);
Status emit_organization_activity_policy_projection(ProjectionWriter&,std::string_view,const OrganizationActivityPolicy&);
Outcome<BoundaryValue> encode_organization_activity_policy(const OrganizationActivityPolicy&);
// 对外作者入口必须完整核真实注册和受信上限，缺任一依赖不产生“已准入目录”。
Outcome<OrganizationActivityPolicy> decode_organization_activity_policy(const BoundaryValue&,const CatalogIdentity&,
 const OrganizationActivityPolicyOriginalRead&,const OrganizationActivityPolicyLimitRead&);
}
