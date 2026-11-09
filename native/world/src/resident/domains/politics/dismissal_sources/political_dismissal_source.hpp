// 原撤职的具名历史权源候选；只读Office/Grant与原失效因果，不注册格式或另写任免。
#pragma once
#include "resident/domains/politics/politics_model.hpp"
#include "resident/domains/politics/politics_territory_contract.hpp"
namespace hanmo::resident {
// 首片明确原同治所上级Office与同组织目标辖区；选定零资源专项Grant，不能借旧may_delegate扩权。
struct PoliticalDismissalSourcePolicy {
 StableId id,jurisdiction_id,issuer_office_id,grant_id;
};
// 原writer前后捕获的拥有型来源：原politics事实作主键，不生成第二次撤职或Office事实。
// 独立来源段保存本结构；actual result、本人生命/成员/控制由完整层核验。
struct PoliticalDismissalSource {
 StableId policy_id,dismissal_fact_id,office_change_fact_id,grant_id,removed_actor_id;
 ActingAuthority authority;
 Integer tick=0,jurisdiction_revision=0,previous_target_epoch=0;
};
// 返回同候选读戳与已经重建的原签字/被免任期；不宣称当前角色或当前持有人仍是历史人物。
struct PoliticalDismissalProvenance {
 WorldIdentity identity;
 StableId dismissal_fact_id,office_change_fact_id,issuer_actor_id,removed_actor_id;
 Integer observed_tick=0,issuer_epoch=0,target_epoch=0;
};
// 精确原撤职共同头与规范分配号；不把其他旧politics事实当作撤职。
std::optional<Integer> political_dismissal_fact_sequence(const CommonStore&,const StableId&);
// 纯目录门不看今日任职；完整原Politics/Organization目录须先由各自域验证。
Status validate_political_dismissal_source_policy(const PoliticsCatalog&,const OrganizationCatalog&,const PoliticalDismissalSourcePolicy&);
// 当前口仍实际执行原check_authority，并确定它按Office.grant_ids顺序选择的同一Grant。
// 目录严格零钱货，故本口不伪造公库Owner的历史证明；有资源Grant需要独立真实历史读。
Status validate_political_dismissal_current_authority(const CommonStore&,const PoliticsState&,
 const PoliticsCatalog&,const OrganizationState&,const OrganizationCatalog&,
 const PoliticalDismissalSourcePolicy&,const PoliticalDismissCommand&);
// 稳定历史口只认原 political_office_dismissed/politics 与其紧邻唯一 invalidate_office。
// 原Common/Organization/Politics完整先验仍必要；本口不回调全World/Task/私业，不授新命令。
Outcome<PoliticalDismissalProvenance> read_political_dismissal_provenance(const CommonStore&,
 const PoliticsState&,const PoliticsCatalog&,const OrganizationState&,const OrganizationCatalog&,
 const PoliticalDismissalSourcePolicy&,const PoliticalDismissalSource&,
 const PoliticalTerritoryRead* territories=nullptr);
}
