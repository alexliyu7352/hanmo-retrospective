// 公共接待岗位另行显式认领；原Office换任、私人生前控制和旧文书custody均不自动改岗位。
#pragma once
#include "resident/domains/politics/succession/political_succession_nominations.hpp"
namespace hanmo::resident {
inline constexpr const char* political_office_reception_domain="political_office_receptions";
inline constexpr const char* political_office_reception_command="political_assume_office_reception";
// 首片只允许原首府的纯information岗位；原有OrgDuty和承包/教学/随行服务不在本段重分配。
struct PoliticalOfficeReceptionPolicy {StableId id,succession_policy_id,role_id,reception_grant_id;};
struct PoliticalOfficeReceptionCatalog {Integer version=1;OrderedTable<PoliticalOfficeReceptionPolicy> policies;};
// 只认领本人实际在任的原Office；期望旧岗位人/来源同时阻止过时报价接管另一任的接待席。
struct PoliticalAssumeOfficeReception {ActingAuthority authority;StableId policy_id,expected_role_actor_id,expected_assignment_fact_id;};
using PoliticalOfficeReceptionCommand=PoliticalAssumeOfficeReception;
// 一个新事实对应一次原Common Role actor替换；不复制原Office账、Dialogue或任何旧Task/拜帖知情。
struct PoliticalOfficeReceptionAssignment {
 StableId id,policy_id,organization_id,office_id,role_id,actor_id,previous_actor_id,previous_assignment_fact_id,office_change_fact_id,grant_id;
 Integer office_epoch=0,tick=0;
 std::optional<PoliticalSuccessionPlayerReceipt> player_receipt;
};
struct PoliticalOfficeReceptionState {Integer version=1;OrderedTable<PoliticalOfficeReceptionAssignment> assignments;};
// 来源仅证明指定岗位持有人；它不授权旧initial_actor锁定的收据、私人同意或公共资金操作。
struct PoliticalOfficeReceptionSource {
 WorldIdentity source_identity;StableId policy_id,organization_id,office_id,role_id,actor_id,assignment_fact_id,before_fact_id;
 Integer assigned_tick=0,observed_tick=0;bool authored=false;
};
// 新域只接受自己的唯一规范真实事实头；其它域消费历史须另有具名边界合同。
std::optional<Integer> political_office_reception_fact_sequence(const CommonStore&,const StableId&);
Status validate_political_office_reception_catalog(const CommonStore&,const CommonCatalog&,const OrganizationCatalog&,const PoliticalSuccessionCatalog&,const PoliticalOfficeReceptionCatalog&);
Outcome<PoliticalOfficeReceptionCatalog> decode_political_office_reception_catalog(const BoundaryValue&);
BoundaryValue encode_political_office_reception_catalog(const PoliticalOfficeReceptionCatalog&);
// 纯历史入口先核原Office全链，再取本岗位认领事实前界，不把今日任职倒填旧接待资格。
Outcome<PoliticalSuccessionOfficeSource> read_political_office_reception_office_before(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const PoliticalSuccessionPolicy&,Integer,const StableId&);
// 同候选完整本域历史先验后使用；当前Role actor必须恰为初始或最后真实认领人。
Outcome<PoliticalOfficeReceptionSource> read_political_office_reception(const CommonStore&,const CommonCatalog&,const PoliticalSuccessionCatalog&,const PoliticalOfficeReceptionCatalog&,const PoliticalOfficeReceptionState&,const StableId& policy_id);
// 只可在同一岗位自己的真实认领之前读取；不会给任意外域fact一个万能历史Role口。
Outcome<PoliticalOfficeReceptionSource> read_political_office_reception_before_assignment(const CommonStore&,const CommonCatalog&,const PoliticalSuccessionCatalog&,const PoliticalOfficeReceptionCatalog&,const PoliticalOfficeReceptionState&,const StableId& policy_id,Integer tick,const StableId& actual_assignment);
}
