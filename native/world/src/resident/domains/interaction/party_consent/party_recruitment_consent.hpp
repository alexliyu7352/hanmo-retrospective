// 当面招伴只借对方真实说话节点的明确本人声明；不造第二成员表或雇用工资账。
#pragma once
#include "resident/domains/interaction/dialogue_provider_source.hpp"
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident {
// 固定双方与原say节点；decision是原作者声明，玩家命令不能提交替代答案。
struct PartyRecruitmentDeclaration {
    StableId id,leader_actor_id,companion_actor_id,role_id,dialogue_id,node_id,decision;
};
struct PartyRecruitmentConsentCatalog {Integer version=1;std::vector<PartyRecruitmentDeclaration> declarations;};
// 只在当前世界/会话内借用。加入仍须原join_party的信任、占用、Party人数与费用检查。
struct PartyRecruitmentConsentQuote {
    AuthoredProviderSourceProof source;
    StableId declaration_id,decision,role_id;
    Integer cost_per_tick=0;
};
// 每个被显式保护的岗位必须通过原provider声明；未配置的旧岗位保持原规则。
Outcome<std::optional<PartyRecruitmentConsentQuote>> preview_party_recruitment_consent(
    const CommonStore&,const InteractionState&,const InteractionCatalog&,
    const PartyRecruitmentConsentCatalog*,const InteractionAccess&,
    const StableId& leader,const InteractionContext&);
// 提交前重查同一say、本人及作者定义，不能把旧愿意用于新报价或当前拒绝节点。
Status revalidate_party_recruitment_consent(const CommonStore&,const InteractionState&,
    const InteractionCatalog&,const PartyRecruitmentConsentCatalog*,const InteractionAccess&,
    const PartyRecruitmentConsentQuote&);
Status validate_party_recruitment_consent_catalog(const PartyRecruitmentConsentCatalog&,const InteractionCatalog&);
Outcome<PartyRecruitmentConsentCatalog> decode_party_recruitment_consent_catalog(const BoundaryValue&,const InteractionCatalog&);
BoundaryValue encode_party_recruitment_consent_catalog(const PartyRecruitmentConsentCatalog&);
// 本模块只有作者准入marker，没有新Party、钱款或冷状态段；未选profile不能夹带目录。
Outcome<bool> decode_party_recruitment_consent_profile(const BoundaryValue&,const std::vector<StableId>& capabilities);
Status validate_party_recruitment_consent_layout(const std::vector<std::pair<StableId,StableId>>&,const PartyRecruitmentConsentCatalog*);
// 只展示当前原会话已说出的声明；愿意仍要通过原join_party报价并由当前玩家确认。
Outcome<BoundaryValue> query_party_recruitment_consent(const CommonStore&,const InteractionState&,const InteractionCatalog&,const PartyRecruitmentConsentCatalog*,const InteractionAccess&);
}
