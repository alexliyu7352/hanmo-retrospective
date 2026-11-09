// 明确本人打开原出生者模板，复用原InteractionSession/DialogueNext/Choose/End，不制造NPC同意。
#pragma once
#include "born_personal_dialogue_contract.hpp"
namespace hanmo::resident {
struct OpenBornPersonalDialogue { StableId provider_actor_id,template_id; };
struct BornPersonalDialogueQuote { WorldIdentity source_identity;StableId listener_actor_id,template_id,dialogue_id;InteractionContext context;BornActorSource born; };
const BornPersonalDialogueTemplate* born_personal_dialogue_template(const BornPersonalDialogueCatalog&,const StableId&);
Outcome<BornPersonalDialogueQuote> preview_born_personal_dialogue(const CommonStore&,const InteractionState&,const BornPersonalDialogueState&,const InteractionCatalog&,const BornPersonalDialogueCatalog&,const InteractionAccess&,const BornPersonalDialogueSources&,const StableId& listener,const OpenBornPersonalDialogue&);
// 外层World候选持Common和完整Interaction保存点；末ID或pump失败必须整体撤回，不能继续复用候选。
Outcome<StableId> open_born_personal_dialogue_in_candidate(CommonStore&,InteractionState&,BornPersonalDialogueState&,const InteractionCatalog&,const BornPersonalDialogueCatalog&,const InteractionAccess&,const BornPersonalDialogueSources&,const StableId& listener,const OpenBornPersonalDialogue&,const BornPersonalDialogueQuote&);
// 精确原会话绑定允许原pump的同候选stepping过渡；完整域/Interaction冷门只准say/choice。
// 全部出生/控制史由晚门完整核，不能以此结构识别单独冷放行。
Outcome<bool> match_born_personal_dialogue_session(const CommonStore&,const BornPersonalDialogueState&,const BornPersonalDialogueCatalog&,const InteractionCatalog&,const InteractionSession&);
Status validate_born_personal_dialogues(const CommonStore&,const InteractionState&,const BornPersonalDialogueState&,const InteractionCatalog&,const BornPersonalDialogueCatalog&,const BornPersonalDialogueSources&);
class BornPersonalDialogueAccess final : public BornPersonalDialogueRead {
public:
    BornPersonalDialogueAccess(const CommonStore&,const InteractionState&,const BornPersonalDialogueState&,const InteractionCatalog&,const BornPersonalDialogueCatalog&,const InteractionAccess&,const BornPersonalDialogueSources&);
    BornPersonalDialogueAccess(const BornPersonalDialogueAccess&)=delete;BornPersonalDialogueAccess& operator=(const BornPersonalDialogueAccess&)=delete;
    Outcome<BornPersonalDialogueProof> born_personal_dialogue(const StableId&,const StableId&,const StableId&) const override;
    Status born_personal_dialogue_before_fact(const BornPersonalDialogueProof&,Integer,const StableId&) const override;
private:
    const CommonStore& common_;const InteractionState& interactions_;const BornPersonalDialogueState& state_;const InteractionCatalog& interaction_catalog_;const BornPersonalDialogueCatalog& catalog_;const InteractionAccess& interaction_access_;const BornPersonalDialogueSources& sources_;
};
}
