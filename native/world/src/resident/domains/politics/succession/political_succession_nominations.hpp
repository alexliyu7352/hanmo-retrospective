// 生前首职继任意愿账：原任职人专项签发、候选本人接受/拒绝，各自不产生Office变更。
#pragma once
#include "political_succession_contract.hpp"
#include "resident/domains/family/family_death.hpp"
namespace hanmo::resident {
// 首片所有写入均来自玩家原提交；实际提交尾尚未接入前不得注册此账。
struct PoliticalSuccessionPlayerReceipt {StableId actor_id,result_id;Integer request_sequence=0,result_revision=0;};
struct PoliticalSuccessionNomination {
 StableId id,policy_id,organization_id,office_id,issuer_actor_id,candidate_actor_id,grant_id;
 Integer office_epoch=0,issued_tick=0,response_until=0;
 std::optional<PoliticalSuccessionPlayerReceipt> player_receipt;
};
// 本人拒绝可撤回已接受的未来负担；必须指向原接受，绝不注销已经生效的公职。
struct PoliticalSuccessionAnswer {
 StableId id,nomination_fact_id,actor_id,previous_acceptance_fact_id;
 Integer tick=0;bool accepted=false;
 std::optional<PoliticalSuccessionPlayerReceipt> player_receipt;
};
struct PoliticalSuccessionWithdrawal {
 StableId id,nomination_fact_id,actor_id,grant_id;Integer office_epoch=0,tick=0;
 std::optional<PoliticalSuccessionPlayerReceipt> player_receipt;
};
// 正式继位只引用原真实Office变更，持有人与公库仍在原组织/Common唯一账。
struct PoliticalSuccessionAccession {
 StableId id,acceptance_fact_id,actor_id,death_fact_id,notice_binding_fact_id,office_change_fact_id;
 Integer tick=0,installed_office_epoch=0;
 std::optional<PoliticalSuccessionPlayerReceipt> player_receipt;
};
struct PoliticalSuccessionAccessionState {Integer version=1;OrderedTable<PoliticalSuccessionAccession> accessions;};
struct PoliticalSuccessionNominationState {
 Integer version=1;
 OrderedTable<PoliticalSuccessionNomination> nominations;
 OrderedTable<PoliticalSuccessionAnswer> answers;
 OrderedTable<PoliticalSuccessionWithdrawal> withdrawals;
};
// 此权源只核作者原首府当时仍可由原组织办理；World须提供真实控制史，缺读即拒。
using PoliticalSuccessionSeatRead=std::function<Status(const PoliticalSuccessionPolicy&,Integer,const StableId& actual_boundary)>;
using PoliticalSuccessionPlayerRead=std::function<Outcome<StableId>(Integer,const StableId& actual_boundary)>;
struct PoliticalSuccessionNominationContext {
 const CommonStore& common;const CommonCatalog& common_catalog;
 const OrganizationState& organizations;const OrganizationCatalog& organization_catalog;
 const LocalEconomyState& economy;const ActivityRead& activity;
 const PoliticalSuccessionCatalog& catalog;const PoliticalSuccessionNominationState& state;
 FamilyDeathOriginRead death_origin;PoliticalSuccessionSeatRead seat;PoliticalSuccessionPlayerRead player_before;
 const PoliticalSuccessionAccessionState* accessions=nullptr;
};
using PoliticalSuccessionNominationCommand=std::variant<PoliticalNominateSuccessor,PoliticalAnswerSuccession,PoliticalWithdrawSuccession>;
struct PoliticalSuccessionNominationQuote {WorldIdentity identity;StableId actor_id;PoliticalSuccessionNominationCommand command;};
Status validate_political_succession_nominations(const PoliticalSuccessionNominationContext&);
Outcome<PoliticalSuccessionNominationQuote> preview_political_succession_nomination(const PoliticalSuccessionNominationContext&,const StableId& actor,const PoliticalSuccessionNominationCommand&);
// 调用者同一Common/本账候选保存点覆盖所有失败；不触任职、公库、军事、Family控制或私产。
Outcome<StableId> apply_political_succession_nomination_in_candidate(CommonStore&,PoliticalSuccessionNominationState&,const PoliticalSuccessionNominationContext&,const StableId&,const PoliticalSuccessionNominationCommand&,const PoliticalSuccessionNominationQuote&);
}
