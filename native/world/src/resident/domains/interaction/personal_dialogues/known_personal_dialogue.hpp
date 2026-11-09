// 本人显式开启眼前真实人物模板；全程复用原InteractionSession/Next/Choose/End。
#pragma once
#include "known_personal_dialogue_catalog.hpp"
#include "known_personal_dialogue_state.hpp"
#include "known_personal_actor_source.hpp"
namespace hanmo::resident {
struct KnownPersonalDialogueContext {
    const CommonStore& common;
    const InteractionState& interactions;
    const KnownPersonalDialogueState& state;
    const InteractionCatalog& definitions;
    const KnownPersonalDialogueCatalog& catalog;
    const FamilyCatalog& family_catalog;
    const BirthArrangementCatalog& birth_catalog;
    const InteractionAccess& interaction_access;
    const KnownPersonalDialogueSources& sources;
};
struct OpenKnownPersonalDialogue { StableId provider_actor_id,template_id; };
struct KnownPersonalDialogueQuote {
    WorldIdentity source_identity;StableId listener_actor_id,template_id,dialogue_id;
    InteractionContext context;KnownPersonalActorOrigin listener_origin,provider_origin;
};
Outcome<KnownPersonalDialogueQuote> preview_known_personal_dialogue(const KnownPersonalDialogueContext&,const StableId& listener,const OpenKnownPersonalDialogue&);
// 原Common和完整Interaction保存点由外层持有，失败不得继续提交这个候选。
Outcome<StableId> open_known_personal_dialogue_in_candidate(CommonStore&,InteractionState&,KnownPersonalDialogueState&,const KnownPersonalDialogueContext&,const StableId&,const OpenKnownPersonalDialogue&,const KnownPersonalDialogueQuote&);
// 仅原pump允许stepping过渡；完整域/Interaction冷验仍只能say/choice。
Outcome<bool> match_known_personal_dialogue_session(const CommonStore&,const KnownPersonalDialogueState&,const KnownPersonalDialogueCatalog&,const InteractionCatalog&,const InteractionSession&);
Status validate_known_personal_dialogues(const KnownPersonalDialogueContext&);
class KnownPersonalDialogueAccess final:public KnownPersonalDialogueRead {
public:
    explicit KnownPersonalDialogueAccess(const KnownPersonalDialogueContext&);
    KnownPersonalDialogueAccess(const KnownPersonalDialogueAccess&)=delete;KnownPersonalDialogueAccess& operator=(const KnownPersonalDialogueAccess&)=delete;
    Outcome<KnownPersonalDialogueProof> known_personal_dialogue(const StableId&,const StableId&,const StableId&) const override;
    Status known_personal_dialogue_before_fact(const KnownPersonalDialogueProof&,Integer,const StableId&) const override;
private:
    KnownPersonalDialogueContext context_;
};
}
