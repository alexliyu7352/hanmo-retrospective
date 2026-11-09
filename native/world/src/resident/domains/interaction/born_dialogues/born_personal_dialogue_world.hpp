// 同一World候选栈内借用原出生/死因/控制读；不持第二人物账，不反调Legacy2完整验证。
#pragma once
#include "born_personal_dialogue_view.hpp"
namespace hanmo::resident {
struct WorldBundle;
class BornPersonalDialogueWorldAccess final {
public:
    static Outcome<std::shared_ptr<BornPersonalDialogueWorldAccess>> create(const WorldBundle&);
    BornPersonalDialogueWorldAccess(const BornPersonalDialogueWorldAccess&)=delete;BornPersonalDialogueWorldAccess& operator=(const BornPersonalDialogueWorldAccess&)=delete;
    const BornPersonalDialogueRead& read() const{return read_;}
    const BornPersonalDialogueSources& sources() const{return sources_;}
    const InteractionAccess& interaction() const{return interaction_;}
private:
    explicit BornPersonalDialogueWorldAccess(const WorldBundle&);
    const WorldBundle& world_;FamilyDeathOriginRead deaths_;BornPersonalDialogueSources sources_;InteractionAccess interaction_;BornPersonalDialogueAccess read_;
};
Outcome<bool> match_born_personal_dialogue_session_world(const WorldBundle&,const InteractionSession&);
Status validate_born_personal_dialogue_world(const WorldBundle&);
Outcome<BornPersonalDialogueQuote> prepare_born_personal_dialogue_world(const WorldBundle&,const StableId&,const OpenBornPersonalDialogue&);
Outcome<InteractionEffect> commit_born_personal_dialogue_world(WorldBundle&,const StableId&,const OpenBornPersonalDialogue&,const BornPersonalDialogueQuote&);
Outcome<BoundaryValue> query_born_personal_dialogue_world(const WorldBundle&,Integer cursor=0);
Outcome<StableId> born_personal_dialogue_new_result_world(const WorldBundle& before,const WorldBundle& after,const StableId& actor,const StableId& kind,const ReceiptDetails&);
Status bind_born_personal_dialogue_result_world(WorldBundle&,const StableId& source,const StableId& actor,Integer request,const CommitResult&);
}
