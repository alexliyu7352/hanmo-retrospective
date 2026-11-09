// 后任生前交接账冻结本任真实根、双方实际意愿与原Office变更引用，钱货、军令和玩家控制仍归原域。
#pragma once
#include "political_term_handover_contract.hpp"
#include "resident/domains/politics/succession/political_succession_nominations.hpp"
namespace hanmo::resident {
struct PoliticalTermHandoverOffer {
 StableId id,policy_id,organization_id,office_id,issuer_actor_id,candidate_actor_id,grant_id,head_source_fact_id,head_office_change_fact_id;
 Integer office_epoch=0,issued_tick=0,response_until=0;
 std::optional<PoliticalSuccessionPlayerReceipt> player_receipt;
};
// 接受与原Office换任同次写成；拒绝没有Office结果，不保留可稍后反悔成接受的半许可。
struct PoliticalTermHandoverAnswer {
 StableId id,offer_fact_id,actor_id,office_change_fact_id;Integer tick=0,expected_office_epoch=0,installed_office_epoch=0;bool accepted=false;
 std::optional<PoliticalSuccessionPlayerReceipt> player_receipt;
};
struct PoliticalTermHandoverWithdrawal {
 StableId id,offer_fact_id,actor_id,grant_id;Integer office_epoch=0,tick=0;
 std::optional<PoliticalSuccessionPlayerReceipt> player_receipt;
};
struct PoliticalTermHandoverState {Integer version=1;OrderedTable<PoliticalTermHandoverOffer> offers;OrderedTable<PoliticalTermHandoverAnswer> answers;OrderedTable<PoliticalTermHandoverWithdrawal> withdrawals;};
// 所有借口仅同步持有同候选；历史本人/原首府缺来源时拒绝，不能以当前metadata或Owner补齐。
struct PoliticalTermHandoverContext {
 const CommonStore& common;const CommonCatalog& common_catalog;const OrganizationState& organizations;const OrganizationCatalog& organization_catalog;
 const LocalEconomyState& economy;const ActivityRead& activity;const PoliticalHeadSourceContext& heads;const PoliticalSuccessorRoundCatalog& round_catalog;const PoliticalSuccessorRoundState& rounds;const PoliticalTermHandoverCatalog& catalog;const PoliticalTermHandoverState& state;
 FamilyDeathOriginRead death_origin;PoliticalSuccessionSeatRead seat;PoliticalSuccessionPlayerRead player_before;
};
using PoliticalTermHandoverCommand=std::variant<PoliticalOfferTermHandover,PoliticalAnswerTermHandover,PoliticalWithdrawTermHandover>;
struct PoliticalTermHandoverQuote {WorldIdentity identity;StableId actor_id;PoliticalTermHandoverCommand command;};
// 后继新轮与下次生前交接均借此精确原Office原因，不在纯读中重放完整业务资格。
Outcome<std::optional<PoliticalHeadSource>> read_political_term_handover_head_source(const CommonStore&,const PoliticalTermHandoverCatalog&,const PoliticalTermHandoverState&,const PoliticalSuccessorRoundCatalog&,const PoliticalSuccessionPolicy&,const PoliticalSuccessionOfficeSource&);
Status validate_political_term_handover_sources(const PoliticalTermHandoverContext&);
Outcome<PoliticalTermHandoverQuote> preview_political_term_handover(const PoliticalTermHandoverContext&,const StableId&,const PoliticalTermHandoverCommand&);
// 外层同一保存点覆盖Common、Organization和本账；任一晚失败不得发布其中单腿。
Outcome<StableId> apply_political_term_handover_in_candidate(CommonStore&,OrganizationState&,PoliticalTermHandoverState&,const PoliticalTermHandoverContext&,const StableId&,const PoliticalTermHandoverCommand&,const PoliticalTermHandoverQuote&);
}
