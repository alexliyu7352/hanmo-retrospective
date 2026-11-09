// 接管责任账只保原来源和公权决定，资产与实际履约仍由各原领域持有。
#pragma once
#include "politics_takeover_contract.hpp"
#include "hanmo/resident/world_moment.hpp"
namespace hanmo::resident {
inline constexpr const char* political_takeover_pending_fact_kind = "political_public_responsibilities_pending";
inline constexpr const char* political_takeover_decision_fact_kind = "political_public_responsibility_decided";
// 作者显式指定辖区与接任Office；整合代价不能由未知占领补默认值。
struct PoliticalTakeoverRule {
    StableId id, jurisdiction_id, site_id, stronghold_id, garrison_id, successor_office_id;
    PoliticalOccupationRuleSource occupation_rule;
};
struct PoliticalTakeoverCatalog { bool enabled=false; Integer version=1; OrderedTable<PoliticalTakeoverRule> rules; Integer task_execution_version=1; };
// 容器角色来自原责任；只对原公库清单内的容器保存其军事转移前Owner。
struct PoliticalResponsibilityContainer {
    StableId id, role;
    bool inventory=false, selected=false;
    std::optional<StableId> item_id;
    std::optional<Owner> original_owner;
};
struct PoliticalProtectedCustody { StableId inventory_id,lot_id,task_id; };
struct PoliticalAuthoredResponsibilitySource { StableId table,id,parent_id; };
struct PoliticalResponsibilityRecordSource { StableId kind,id; };
// 三种来源分列：作者条目、运行记录与真正发生事实不能互相冒充。
struct PoliticalOriginalResponsibility {
    PoliticalResponsibilityRef responsibility;
    std::optional<SubjectRef> principal, assignee;
    std::vector<SubjectRef> beneficiaries, delivery_beneficiaries;
    std::vector<PoliticalResponsibilityAssetSource> assets;
    std::vector<PoliticalResponsibilityContainer> containers;
    std::vector<PoliticalProtectedCustody> protected_custody;
    std::vector<StableId> source_fact_ids;
    std::vector<PoliticalAuthoredResponsibilitySource> authored_sources;
    std::vector<PoliticalResponsibilityRecordSource> record_sources;
};
// 原军事清单已应用后才登记；该记录不等于已经承担责任或允许收款。
struct PoliticalTakeoverRegistration {
    StableId id, rule_id, occupation_fact_id;
    WorldIdentity source_identity;
    Integer tick=0, captured_next_instance_id=0;
    StableId former_organization_id, successor_organization_id;
    // 清单只引用原军事账；政治域不复制公库余额或货批数量。
    std::vector<StableId> public_wallet_ids,public_inventory_ids;
    std::vector<PoliticalOriginalResponsibility> responsibilities;
    // 仅political_takeover/2保存真实登记append时刻；/1必须为空。
    std::optional<WorldMoment> registered_at;
};
struct PoliticalTakeoverApprovalRecord {
    StableId id, registration_fact_id;
    std::optional<StableId> previous_approval_fact_id;
    ActingAuthority authority;
    StableId grant_id;
    Integer tick=0;
    std::vector<StableId> decision_fact_ids;
    // 原批准签发时刻，不是占领时刻，也不是恢复时今日时刻。
    std::optional<WorldMoment> approved_at;
};
struct PoliticalTakeoverDecisionRecord {
    StableId id, approval_fact_id;
    PoliticalResponsibilityDecision decision;
};
// /1原四根与原字段不变；/2仅在原登记/批准行尾增加真实Moment。
struct PoliticalTakeoverState {
    Integer version=1;
    OrderedTable<PoliticalTakeoverRegistration> registrations;
    OrderedTable<PoliticalTakeoverApprovalRecord> approvals;
    OrderedTable<PoliticalTakeoverDecisionRecord> decisions;
};
// 输入不能重写原主体、私户受益容器或军事资产清单。
struct PoliticalResponsibilityRequest {
    PoliticalResponsibilityRef responsibility;
    PoliticalResponsibilityDisposition disposition=PoliticalResponsibilityDisposition::suspend;
    std::optional<StableId> payment_wallet_id,payment_inventory_id;
    std::vector<StableId> production_asset_ids;
    std::optional<StableId> supply_policy_fact_id;
};
struct PoliticalTakeoverDecisionCommand {
    ActingAuthority authority;
    StableId registration_fact_id;
    WorldIdentity expected_identity;
    std::optional<StableId> expected_previous_approval_fact_id;
    std::vector<PoliticalResponsibilityRequest> decisions;
};
BoundaryValue encode_political_takeover_state(const PoliticalTakeoverState&);
Outcome<PoliticalTakeoverState> decode_political_takeover_state(const BoundaryValue&);
Status visit_political_takeover_checkpoint(CodecVisitor&,PoliticalTakeoverState&);
BoundaryValue encode_political_original_responsibility(const PoliticalOriginalResponsibility&);
BoundaryValue encode_political_responsibility_decision(const PoliticalResponsibilityDecision&);
}
