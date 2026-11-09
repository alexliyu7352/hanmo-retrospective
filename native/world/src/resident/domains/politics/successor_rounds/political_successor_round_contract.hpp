// 已真实就任者的新轮意愿是独立显式制度，不回写原首任继位或生前交接1版。
#pragma once
#include "political_head_source.hpp"
namespace hanmo::resident {
inline constexpr const char* political_successor_round_nominate_command="political_nominate_next_successor";
inline constexpr const char* political_successor_round_answer_command="political_answer_next_succession";
inline constexpr const char* political_successor_round_withdraw_command="political_withdraw_next_successor";
inline constexpr const char* political_successor_round_accede_command="political_accede_next_succession";
// 每个原首职一项新制度和一份专项Grant；零财务权，有限接受/实际死亡后的办理期限。
struct PoliticalSuccessorRoundPolicy {StableId id,succession_policy_id,nomination_grant_id;Integer response_ticks=0,accession_ticks=0;};
struct PoliticalSuccessorRoundCatalog {Integer version=1;OrderedTable<PoliticalSuccessorRoundPolicy> policies;};
// 提名必须显式确认当前真实任职来源和epoch，不能把任意原Office持有人当成继承本制度。
struct PoliticalNominateNextSuccessor {ActingAuthority authority;StableId policy_id,candidate_actor_id,expected_head_source_fact_id;Integer expected_office_epoch=0;};
struct PoliticalAnswerNextSuccession {StableId nomination_fact_id;bool accept=false;};
struct PoliticalWithdrawNextSuccession {ActingAuthority authority;StableId nomination_fact_id;};
struct PoliticalAccedeNextSuccession {StableId acceptance_fact_id,death_fact_id,death_notice_binding_fact_id;Integer expected_office_epoch=0;};
using PoliticalSuccessorRoundCommand=std::variant<PoliticalNominateNextSuccessor,PoliticalAnswerNextSuccession,PoliticalWithdrawNextSuccession,PoliticalAccedeNextSuccession>;
// 原任期来源在签发时固定；另一轮新任、免职或再占领都不能复用旧授权。
struct PoliticalSuccessorRoundNomination {
 StableId id,policy_id,organization_id,office_id,issuer_actor_id,candidate_actor_id,grant_id,head_source_fact_id,head_office_change_fact_id;
 Integer office_epoch=0,issued_tick=0,response_until=0;
 std::optional<PoliticalSuccessionPlayerReceipt> player_receipt;
};
// 原本人可在正式就任前撤回意愿，保原接受事实并明确绑定，不能以删除旧记录表示反悔。
struct PoliticalSuccessorRoundAnswer {StableId id,nomination_fact_id,actor_id,previous_acceptance_fact_id;Integer tick=0;bool accepted=false;std::optional<PoliticalSuccessionPlayerReceipt> player_receipt;};
struct PoliticalSuccessorRoundWithdrawal {StableId id,nomination_fact_id,actor_id,grant_id;Integer office_epoch=0,tick=0;std::optional<PoliticalSuccessionPlayerReceipt> player_receipt;};
// 原Office更职仍是唯一公权结果；本记录仅保存真实接受/死因/实收文书的原来源。
struct PoliticalSuccessorRoundAccession {StableId id,acceptance_fact_id,actor_id,death_fact_id,notice_binding_fact_id,office_change_fact_id;Integer tick=0,installed_office_epoch=0;std::optional<PoliticalSuccessionPlayerReceipt> player_receipt;};
struct PoliticalSuccessorRoundState {Integer version=1;OrderedTable<PoliticalSuccessorRoundNomination> nominations;OrderedTable<PoliticalSuccessorRoundAnswer> answers;OrderedTable<PoliticalSuccessorRoundWithdrawal> withdrawals;OrderedTable<PoliticalSuccessorRoundAccession> accessions;};
// 新任期自己的实际就任也可作为后续任期根；所有引用都先于下一次真实提名。
Outcome<std::optional<PoliticalHeadSource>> read_political_successor_round_head(const PoliticalHeadSourceContext&,const PoliticalSuccessorRoundCatalog&,const PoliticalSuccessorRoundState&,const StableId& policy_id);
Outcome<std::optional<PoliticalHeadSource>> read_political_successor_round_head_before(const PoliticalHeadSourceContext&,const PoliticalSuccessorRoundCatalog&,const PoliticalSuccessorRoundState&,const StableId& policy_id,Integer tick,const StableId& actual_fact);
Status validate_political_successor_round_catalog(const CommonStore&,const CommonCatalog&,const OrganizationCatalog&,const PoliticalSuccessionCatalog&,const PoliticalSuccessorRoundCatalog&);
Outcome<PoliticalSuccessorRoundCatalog> decode_political_successor_round_catalog(const BoundaryValue&);
BoundaryValue encode_political_successor_round_catalog(const PoliticalSuccessorRoundCatalog&);
std::optional<Integer> political_successor_round_fact_sequence(const CommonStore&,const StableId& actual_fact);
}
