// 动态世界跨域值合同；只保存有限引用/观察，不持有第二份资产或任意运行JSON。
#pragma once
#include "hanmo/resident/model_contract.hpp"
#include "hanmo/resident/world_moment.hpp"
#include <functional>

namespace hanmo::resident {
enum class SubjectKind { actor,organization,cohort };
struct SubjectRef { SubjectKind kind=SubjectKind::actor;StableId id; };
struct ScopeRef { enum class Kind { site,region,force,account,activity };Kind kind=Kind::site;StableId id; };
struct ActingAuthority { StableId organization_id,office_id;Integer authority_epoch=0;StableId actor_id; };
struct AuthoredOfficeAuthoritySource {
  CatalogIdentity catalog_identity;
  StableId office_id;
  Integer initial_epoch = 0;
};
struct OfficeChangedAuthoritySource {
  StableId fact_id;
};
using OfficeAuthoritySource =
    std::variant<AuthoredOfficeAuthoritySource, OfficeChangedAuthoritySource>;
struct AuthoredGrantAuthoritySource {
  CatalogIdentity catalog_identity;
  StableId grant_id;
};
struct GrantRevisionAuthoritySource {
  StableId fact_id, grant_id;
  Integer revision = 0;
};
using GrantAuthoritySource =
    std::variant<AuthoredGrantAuthoritySource, GrantRevisionAuthoritySource>;
// /2 binds one real original Grant and Office term; no implicit upgrade of /1.
struct AuthorityProof {
  StableId organization_id, office_id, actor_id, command_kind;
  ScopeRef scope;
  Integer candidate_revision = 0, authority_epoch = 0, amount = 0;
  std::vector<ReadVersion> resource_versions;
  Integer schema_version = 2;
  WorldIdentity source_identity;
  StableId grant_id;
  Integer grant_revision = 0;
  OfficeAuthoritySource office_source;
  GrantAuthoritySource grant_source;
  WorldMoment observed_at;
};

// 每半日开始捕获既存集合；新建立的行动/活动/消息不能取得本步已过去的进度。
struct DynamicStepInput { Integer from_tick=0,to_tick=0;std::vector<StableId> existing_activity_ids,due_message_ids,existing_action_ids; };
struct SupplyObservation { StableId source_key,organization_id,site_id,inventory_id,item_id,source_fact_id;Integer target_amount=0,physical_amount=0,source_revision=0; };
struct ObligationObservation { StableId obligation_id,principal_id,beneficiary_id,source_fact_id;Integer due_tick=0,due_amount=0,paid_amount=0,outstanding_amount=0;StableId arrear_id; };
struct FactParticipant { SubjectRef subject;StableId role; };
struct DynamicFactView { StableId id,kind,rule_version;Integer tick=0;std::vector<StableId> cause_fact_ids,affair_ids;std::vector<FactParticipant> participants;std::optional<SupplyObservation> supply;std::optional<ObligationObservation> obligation;StableId subject_id,outcome;bool publicly_visible=false; };
struct OrganizationMemberView { StableId organization_id,actor_id,response;Integer loyalty=0,unpaid_amount=0,overdue_periods=0;bool accepts_new_duty=false; };

// 机构义务必须登记到实际组织、义务和资源；存在组织ID不等于有权花钱。
class OrganizationRead {
public:
    virtual ~OrganizationRead()=default;
    virtual bool organization_exists(const StableId&) const=0;
    virtual bool organization_active(const StableId&) const=0;
    virtual bool institutional_obligation_allowed(const StableId& issuer_id,const StableId& obligation_id,const StableId& principal_org_id,const StableId& source_container_id,Integer amount,Integer tick) const=0;
    virtual bool actor_accepts_duty(const StableId& actor_id,const StableId& organization_id) const=0;
    virtual std::optional<Integer> authority_epoch(const StableId& office_id) const=0;
    virtual Outcome<AuthorityProof> authorize(const ActingAuthority&,const StableId& command_kind,const ScopeRef&,Integer amount) const=0;
    virtual std::optional<OrganizationMemberView> member_view(const StableId& actor_id,const StableId& organization_id) const=0;
};
// 组织个人职责只读取原行动的永久完成事实；开始态、普通等待和无结果来源的行动不冒充完成。
struct DynamicActionResult { StableId action_id,action_kind,source_fact_id,code;Integer completed_tick=0;bool success=false;std::vector<StableId> participant_actor_ids; };
struct DeliveryFact;
// 只投影已登记真实结果；事实ID保持全局插入顺序，指针只在当前只读片段借用。
class DynamicResultRead {
public:
    virtual ~DynamicResultRead()=default;
    virtual bool supports_action_result(const StableId&) const { return false; }
    virtual std::optional<DynamicActionResult> action_result(const StableId&) const { return {}; }
    virtual const DeliveryFact* delivery(const StableId& fact_id) const=0;
    virtual std::optional<DynamicFactView> fact(const StableId& fact_id) const=0;
    virtual std::vector<StableId> fact_ids(Integer after_tick,Integer through_tick) const=0;
};
struct KnownSubjectView;struct DirectorTransition;struct CommandPayload;
struct DecisionProposal { StableId subject_id,plan_id,reason_fact_id;std::shared_ptr<const CommandPayload> command;std::shared_ptr<const DirectorTransition> transition;std::vector<ReadVersion> expected_versions; };
struct DecisionOutcome { enum class Kind { adopted,rejected,internal_failure };Kind kind=Kind::rejected;StableId code;std::optional<StableId> action_id;std::vector<StableId> receipt_ids;std::optional<StableId> org_activity_id; };
} // 报价与提案在同候选复验，应用/回滚由中央管理，不从读取接口直接提交世界。
