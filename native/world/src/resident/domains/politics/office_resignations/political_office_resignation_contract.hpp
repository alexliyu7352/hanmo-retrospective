// 普通公职的本人辞任申请与上级批准是两次真实行为；不以欠薪离组织或首职继位代替。
#pragma once
#include "resident/domains/politics/succession/political_succession_nominations.hpp"
#include "resident/domains/politics/politics_model.hpp"
namespace hanmo::resident {
inline constexpr const char* political_office_resignation_module="political_office_resignations";
inline constexpr const char* political_office_resignation_request_command="political_request_office_resignation";
inline constexpr const char* political_office_resignation_answer_command="political_answer_office_resignation";
inline constexpr const char* political_office_resignation_withdraw_command="political_withdraw_office_resignation";
// 首版限定原同组织首职批准下属辖区职务；首职让位、接管后续任和副手处置另有原业务。
struct PoliticalOfficeResignationPolicy {
 StableId id,jurisdiction_id,approval_succession_policy_id,approval_grant_id;
 Integer response_ticks=0;
};
struct PoliticalOfficeResignationCatalog {Integer version=1;OrderedTable<PoliticalOfficeResignationPolicy> policies;};
struct PoliticalRequestOfficeResignation {StableId policy_id;Integer expected_office_epoch=0,expected_jurisdiction_revision=0;};
struct PoliticalAnswerOfficeResignation {ActingAuthority authority;StableId request_fact_id;bool accept=false;};
struct PoliticalWithdrawOfficeResignation {StableId request_fact_id;};
using PoliticalOfficeResignationCommand=std::variant<PoliticalRequestOfficeResignation,PoliticalAnswerOfficeResignation,PoliticalWithdrawOfficeResignation>;
// 申请不改Office；答复前仍原任期。接收人/epoch同时冻结，换首长后旧申请不转给新人签字。
struct PoliticalOfficeResignationRequest {
 StableId id,policy_id,organization_id,office_id,actor_id,approver_office_id,approver_actor_id;
 Integer office_epoch=0,approver_epoch=0,jurisdiction_revision=0,tick=0,response_until=0;
 std::optional<PoliticalSuccessionPlayerReceipt> player_receipt;
};
// 批准紧接原organization_office_changed，只使该原职空缺；原成员、债权、Task和资产不在本账复制。
struct PoliticalOfficeResignationAnswer {
 StableId id,request_fact_id,actor_id,grant_id,office_change_fact_id;
 Integer approver_epoch=0,vacant_office_epoch=0,tick=0;bool accepted=false;
 std::optional<PoliticalSuccessionPlayerReceipt> player_receipt;
};
struct PoliticalOfficeResignationWithdrawal {
 StableId id,request_fact_id,actor_id;Integer tick=0;
 std::optional<PoliticalSuccessionPlayerReceipt> player_receipt;
};
// 仅辞任2追加NPC答复来源；旧1不序列化此尾，普通外部命令不能携带作者来源。
struct PoliticalOfficeResignationAuthoredOrigin {
 StableId intent_id,director_policy_id,actor_id,request_fact_id,reason;
};
struct PoliticalOfficeResignationState {
 Integer version=1;OrderedTable<PoliticalOfficeResignationRequest> requests;
 OrderedTable<PoliticalOfficeResignationAnswer> answers;OrderedTable<PoliticalOfficeResignationWithdrawal> withdrawals;
 OrderedTable<PoliticalOfficeResignationAuthoredOrigin> authored_origins;
};
// 仅指定原Office的历史投影，完整组织账仍由原域先验；不缓存第二份当前任职。
struct PoliticalOfficeResignationOfficeSource {
 WorldIdentity source_identity;StableId organization_id,office_id,last_change_fact_id,boundary_fact_id;
 std::optional<StableId> holder_actor_id;Integer office_epoch=0,observed_tick=0;
};
std::optional<Integer> political_office_resignation_fact_sequence(const CommonStore&,const StableId&,const StableId& kind);
Outcome<PoliticalOfficeResignationOfficeSource> read_political_office_resignation_office(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const StableId& office_id);
Outcome<PoliticalOfficeResignationOfficeSource> read_political_office_resignation_office_before(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const StableId& office_id,Integer tick,const StableId& actual_boundary);
Status validate_political_office_resignation_catalog(const CommonStore&,const CommonCatalog&,const PoliticsCatalog&,const OrganizationCatalog&,const PoliticalSuccessionCatalog&,const PoliticalOfficeResignationCatalog&);
Outcome<PoliticalOfficeResignationCatalog> decode_political_office_resignation_catalog(const BoundaryValue&);
BoundaryValue encode_political_office_resignation_catalog(const PoliticalOfficeResignationCatalog&);
}
