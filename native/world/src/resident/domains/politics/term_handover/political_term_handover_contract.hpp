// 已正式继任者的生前让职另立双方条款；新轮死亡继位意愿与Family控制同意均不授此许可。
#pragma once
#include "resident/domains/politics/successor_rounds/political_successor_round_contract.hpp"
namespace hanmo::resident {
inline constexpr const char* political_term_handover_domain="political_term_handovers";
inline constexpr const char* political_term_handover_offer_command="political_offer_term_handover";
inline constexpr const char* political_term_handover_answer_command="political_answer_term_handover";
inline constexpr const char* political_term_handover_withdraw_command="political_withdraw_term_handover";
// 新政策明确原继任制度与另一零预算Grant，不能把既有死亡提名权限当让职权限。
struct PoliticalTermHandoverPolicy {StableId id,successor_round_policy_id,handover_grant_id;Integer response_ticks=0;};
struct PoliticalTermHandoverCatalog {Integer version=1;OrderedTable<PoliticalTermHandoverPolicy> policies;};
// 前任持有本任真实就任根和Office版本；签发不会先腾空原首职。
struct PoliticalOfferTermHandover {ActingAuthority authority;StableId policy_id,candidate_actor_id,expected_head_source_fact_id;Integer expected_office_epoch=0;};
// 候选本人接受瞬间必须再次证明前任仍在位、有同一专项授权且双方到场。
struct PoliticalAnswerTermHandover {StableId offer_fact_id;bool accept=false;Integer expected_office_epoch=0;};
struct PoliticalWithdrawTermHandover {ActingAuthority authority;StableId offer_fact_id;};
// 历史只接本段四类真实业务头；原首轮与新死亡继位的边界许可不隐式扩大。
std::optional<Integer> political_term_handover_fact_sequence(const CommonStore&,const StableId& actual_fact);
// 本业务真实头之前的原Office截面；旧两种继位读口继续拒本段头。
Outcome<PoliticalSuccessionOfficeSource> read_political_term_handover_office_before(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const PoliticalSuccessionPolicy&,Integer tick,const StableId& actual_fact);
// 只在显式本模块选择时借原任职根；旧入口仍只接旧业务头。
Outcome<std::optional<PoliticalHeadSource>> read_political_head_source_before_term_handover(const PoliticalHeadSourceContext&,const StableId& succession_policy_id,Integer tick,const StableId& actual_fact);
Outcome<std::optional<PoliticalHeadSource>> read_political_successor_round_head_before_term_handover(const PoliticalHeadSourceContext&,const PoliticalSuccessorRoundCatalog&,const PoliticalSuccessorRoundState&,const StableId& round_policy_id,Integer tick,const StableId& actual_fact);
Status validate_political_term_handover_catalog(const CommonStore&,const CommonCatalog&,const OrganizationCatalog&,const PoliticalSuccessionCatalog&,const PoliticalSuccessorRoundCatalog&,const PoliticalTermHandoverCatalog&);
Outcome<PoliticalTermHandoverCatalog> decode_political_term_handover_catalog(const BoundaryValue&);
BoundaryValue encode_political_term_handover_catalog(const PoliticalTermHandoverCatalog&);
}
