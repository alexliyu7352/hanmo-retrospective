// 本人接受/撤回和提供者真实计时照护分流；客户端只给原ID，不提交粮数、替代身份或死亡证明。
#pragma once
#include "child_care_relief_action.hpp"
#include "family_model.hpp"
namespace hanmo::resident {
struct ChildCareReliefCommand {StableId kind,offer_id,child_actor_id,accepted_id,action_id;};
struct ChildCareReliefCommandQuote {WorldIdentity identity;ChildCareReliefCommand command;std::optional<ChildCareReliefQuote> source;std::optional<ChildCareReliefActionQuote> care;};
struct ChildCareReliefCommandResult {FamilyEffect effect;StableId release_action_id;};
Outcome<ChildCareReliefCommand> decode_child_care_relief_command(const StableId&,const BoundaryValue&);
Outcome<ChildCareReliefCommandQuote> preview_child_care_relief_command(const CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const ChildCareReliefCatalog&,const FamilyBirthState&,const FamilyDeathState&,const ChildCareReliefState&,const ChildCareReliefAccess&,const StableId& actor,const ChildCareReliefCommand&);
Status revalidate_child_care_relief_quote(const CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const ChildCareReliefCatalog&,const FamilyBirthState&,const FamilyDeathState&,const ChildCareReliefState&,const ChildCareReliefAccess&,const StableId& actor,const ChildCareReliefCommand&,const ChildCareReliefCommandQuote&);
// 仅零时本人命令。计时care必须走原CommonAction；取消所返原Action由外层同候选释放并恢复原waiting。
Outcome<ChildCareReliefCommandResult> apply_child_care_relief_command_in_candidate(CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const ChildCareReliefCatalog&,const FamilyBirthState&,const FamilyDeathState&,ChildCareReliefState&,const ChildCareReliefAccess&,const StableId& actor,const ChildCareReliefCommand&,const ChildCareReliefCommandQuote&);
BoundaryValue encode_child_care_relief_quote(const ChildCareReliefCommandQuote&);
Status validate_child_care_relief_command_receipt(const ChildCareReliefState&,const StableId& kind,const StableId& actor,Integer tick,const FamilyEffect&);
Outcome<BoundaryValue> query_child_care_relief_page(const CommonStore&,const CommonCatalog&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const ChildCareReliefCatalog&,const FamilyBirthState&,const FamilyDeathState&,const ChildCareReliefState&,const ChildCareReliefAccess&,Integer page);
}
