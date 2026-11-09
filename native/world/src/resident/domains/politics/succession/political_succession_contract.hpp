// 正式政治继位只引用原组织、首职和Office历史；提名与同意均不立即取得公权或私产。
#pragma once
#include "resident/domains/organization/organization_model.hpp"
namespace hanmo::resident {
inline constexpr const char* political_succession_domain="political_successions";
inline constexpr const char* political_succession_nominate_command="political_nominate_successor";
inline constexpr const char* political_succession_withdraw_command="political_withdraw_successor";
// 首片只办理真实空缺的原首职；副手临时代办不是本合同的正式继位结果。
struct PoliticalSuccessionPolicy {
    StableId id,organization_id,leader_office_id,seat_place_id,nomination_grant_id;
    Integer response_ticks=0,accession_ticks=0;
    StableId jurisdiction_id;
};
struct PoliticalSuccessionCatalog {Integer version=1;OrderedTable<PoliticalSuccessionPolicy> policies;};
// 此来源只证明原Office截面，不把当前持有人、空缺或副手身份当作已获正式继位许可。
struct PoliticalSuccessionOfficeSource {
    WorldIdentity source_identity;
    StableId policy_id,organization_id,office_id,seat_place_id,original_leader_actor_id;
    StableId latest_office_fact_id,before_fact_id;
    std::optional<StableId> holder_actor_id;
    Integer office_epoch=0,observed_tick=0;
    bool vacant=false;
};
// 命令对象只表达本人选择；新业务writer尚未登记，不得仅凭本头直接改原Office。
struct PoliticalNominateSuccessor {ActingAuthority authority;StableId policy_id,candidate_actor_id;Integer expected_office_epoch=0;};
struct PoliticalAnswerSuccession {StableId nomination_fact_id;bool accept=false;};
struct PoliticalWithdrawSuccession {ActingAuthority authority;StableId nomination_fact_id;};
struct PoliticalAccedeSuccession {StableId acceptance_fact_id,death_fact_id,death_notice_binding_fact_id;Integer expected_office_epoch=0;};
// 精确五类本域事实头；真实payload tick仍由消费方逐本表核验。
std::optional<Integer> political_succession_fact_sequence(const CommonStore&,const StableId&);
// 完整原组织/公职来源须先验；历史入口只允许本业务的真实具名事实前界。
Outcome<PoliticalSuccessionOfficeSource> read_political_succession_office(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const PoliticalSuccessionPolicy&);
Outcome<PoliticalSuccessionOfficeSource> read_political_succession_office_before(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const PoliticalSuccessionPolicy&,Integer tick,const StableId& actual_fact);
Status validate_political_succession_catalog(const CommonStore&,const CommonCatalog&,const OrganizationCatalog&,const PoliticalSuccessionCatalog&);
Outcome<PoliticalSuccessionCatalog> decode_political_succession_catalog(const BoundaryValue&);
BoundaryValue encode_political_succession_catalog(const PoliticalSuccessionCatalog&);
} // 未登记profile、持久段或Common位；缺少完整业务源时不得授予继位权限。
