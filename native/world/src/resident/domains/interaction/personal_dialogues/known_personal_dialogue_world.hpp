// 同World栈内持有所有借用；原出生/生命先验后只折叠控制链，供后继本人业务单向消费。
#pragma once
#include "known_personal_dialogue_view.hpp"
#include "resident/domains/family/birth_arrangement_source.hpp"
namespace hanmo::resident {
struct WorldBundle;
class KnownPersonalDialogueWorldAccess final {
public:
 static Outcome<std::shared_ptr<KnownPersonalDialogueWorldAccess>> create(const WorldBundle&);
 // 只供原单步已开始业务核旧opening/proof；不允许以此holder开启新会面或读当前SAY。
 static Outcome<std::shared_ptr<KnownPersonalDialogueWorldAccess>> create_for_phase_history(const WorldBundle&,const DynamicStepInput&);
 KnownPersonalDialogueWorldAccess(const KnownPersonalDialogueWorldAccess&)=delete;KnownPersonalDialogueWorldAccess& operator=(const KnownPersonalDialogueWorldAccess&)=delete;
 const KnownPersonalDialogueRead& read()const{return read_;}
 const KnownPersonalDialogueContext& context()const{return context_;}
 const KnownPersonalDialogueCatalog& catalog()const{return context_.catalog;}
private:
 explicit KnownPersonalDialogueWorldAccess(const WorldBundle&,std::optional<DynamicStepInput> phase=std::nullopt);
 const WorldBundle& world_;std::optional<DynamicStepInput> phase_;FamilyDeathOriginRead deaths_;BirthArrangementCatalog empty_birth_;KnownPersonalDialogueSources sources_;InteractionAccess interaction_;KnownPersonalDialogueContext context_;KnownPersonalDialogueAccess read_;
};
Outcome<bool> match_known_personal_dialogue_session_world(const WorldBundle&,const InteractionSession&);
Status validate_known_personal_dialogue_world(const WorldBundle&);
Outcome<KnownPersonalDialogueQuote> prepare_known_personal_dialogue_world(const WorldBundle&,const StableId&,const OpenKnownPersonalDialogue&);
Outcome<InteractionEffect> commit_known_personal_dialogue_world(WorldBundle&,const StableId&,const OpenKnownPersonalDialogue&,const KnownPersonalDialogueQuote&);
Outcome<BoundaryValue> query_known_personal_dialogue_world(const WorldBundle&,Integer cursor=0);
Outcome<StableId> known_personal_dialogue_new_result_world(const WorldBundle& before,const WorldBundle& after,const StableId&,const StableId&,const ReceiptDetails&);
Status bind_known_personal_dialogue_result_world(WorldBundle&,const StableId&,const StableId&,Integer,const CommitResult&);
}
