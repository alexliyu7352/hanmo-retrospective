// 独立生前交接账只保存双方实际意愿与原Office变更引用，钱货、军令和玩家控制仍归原域。
#pragma once
#include "political_head_handover_contract.hpp"
#include "resident/domains/politics/succession/political_succession_nominations.hpp"
#include "resident/domains/organization/organization_runtime.hpp"
#include "resident/domains/family/life_age.hpp"
namespace hanmo::resident {
class PoliticalTakeoverTerritoryAccess;
struct PoliticalHeadHandoverExactIssue {
 WorldMoment occurred_at;std::optional<AuthorityBasisRef> authority_basis;
};
struct PoliticalHeadHandoverOffer {
 StableId id,policy_id,organization_id,office_id,issuer_actor_id,candidate_actor_id,grant_id;
 Integer office_epoch=0,issued_tick=0,response_until=0;
 std::optional<PoliticalSuccessionPlayerReceipt> player_receipt;
 std::optional<PoliticalHeadHandoverExactIssue> exact_issue;
};
// 接受与原Office换任同次写成；拒绝没有Office结果，不保留可稍后反悔成接受的半许可。
struct PoliticalHeadHandoverAnswer {
 StableId id,offer_fact_id,actor_id,office_change_fact_id;Integer tick=0,expected_office_epoch=0,installed_office_epoch=0;bool accepted=false;
 std::optional<PoliticalSuccessionPlayerReceipt> player_receipt;
 std::optional<PoliticalHeadHandoverExactIssue> exact_issue;
};
struct PoliticalHeadHandoverWithdrawal {
 StableId id,offer_fact_id,actor_id,grant_id;Integer office_epoch=0,tick=0;
 std::optional<PoliticalSuccessionPlayerReceipt> player_receipt;
 std::optional<PoliticalHeadHandoverExactIssue> exact_issue;
};
struct PoliticalHeadHandoverState {Integer version=1;OrderedTable<PoliticalHeadHandoverOffer> offers;OrderedTable<PoliticalHeadHandoverAnswer> answers;OrderedTable<PoliticalHeadHandoverWithdrawal> withdrawals;};
struct PoliticalHeadHandoverExactSources {
 const OrganizationRuntime& runtime;const OrganizationAuthorityClock& clock;const LifeAgeSources& ages;
 const ReadStamp& stamp;const ReadStampAuthority& stamp_authority;const DomainReadVersionSource& versions;
 const PoliticalTakeoverTerritoryAccess& territory;
};
// 所有借口仅同步持有同候选；历史本人/原首府缺来源时拒绝，不能以当前metadata或Owner补齐。
struct PoliticalHeadHandoverContext {
 const CommonStore& common;const CommonCatalog& common_catalog;const OrganizationState& organizations;const OrganizationCatalog& organization_catalog;
 const LocalEconomyState& economy;const ActivityRead& activity;const PoliticalSuccessionCatalog& succession_catalog;const PoliticalHeadHandoverCatalog& catalog;const PoliticalHeadHandoverState& state;
 FamilyDeathOriginRead death_origin;PoliticalSuccessionSeatRead seat;PoliticalSuccessionPlayerRead player_before;
 const PoliticalHeadHandoverExactSources* exact=nullptr;
};
using PoliticalHeadHandoverCommand=std::variant<PoliticalOfferHeadHandover,PoliticalAnswerHeadHandover,PoliticalWithdrawHeadHandover>;
struct PoliticalHeadHandoverQuote {WorldIdentity identity;StableId actor_id;PoliticalHeadHandoverCommand command;};
Status validate_political_head_handover_sources(const PoliticalHeadHandoverContext&);
Outcome<PoliticalHeadHandoverQuote> preview_political_head_handover(const PoliticalHeadHandoverContext&,const StableId&,const PoliticalHeadHandoverCommand&);
// 外层同一保存点覆盖Common、Organization和本账；任一晚失败不得发布其中单腿。
Outcome<StableId> apply_political_head_handover_in_candidate(CommonStore&,OrganizationState&,PoliticalHeadHandoverState&,const PoliticalHeadHandoverContext&,const StableId&,const PoliticalHeadHandoverCommand&,const PoliticalHeadHandoverQuote&,OrganizationRuntime* exact_runtime=nullptr);
}
