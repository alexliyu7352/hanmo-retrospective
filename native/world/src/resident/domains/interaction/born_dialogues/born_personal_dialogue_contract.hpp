// 具名出生模板证明只说明实际本人曾在原会话说话；接受、撤回与控制权仍由消费领域解释。
#pragma once
#include "born_personal_dialogue_catalog.hpp"
#include "resident/domains/interaction/dialogue_provider_source.hpp"
#include "resident/domains/family/born_actor_source.hpp"
namespace hanmo::resident {
struct BornPersonalDialogueProof {
    StableId opening_fact_id,template_id;
    AuthoredProviderSourceProof declaration;
    BornActorSource born;
};
// 历史调用者必须先验自己的真实payload.tick，再以原保存proof和实际业务头复核；禁止选择另一场会面替代。
class BornPersonalDialogueRead {
public:
    virtual ~BornPersonalDialogueRead()=default;
    virtual Outcome<BornPersonalDialogueProof> born_personal_dialogue(const StableId& listener,const StableId& provider,const StableId& template_id) const=0;
    virtual Status born_personal_dialogue_before_fact(const BornPersonalDialogueProof&,Integer tick,const StableId& actual_fact) const=0;
};
// 同候选完整Family/Creation/Birth/Lineage/Life先验；回调不反入World或Legacy2消费者。
struct BornPersonalDialogueSources {
    BornActorSourceRead born;
    BornActorSourceBeforeRead born_before;
    std::function<Outcome<ActorPresenceRead>(const StableId&,Integer,const StableId&)> presence_before;
    std::function<Outcome<StableId>(Integer,const StableId&)> player_before;
};
// 历史证明单独随消费方的新版本保存；旧AuthoredNodeSourceProof不增加字段。
BoundaryValue encode_born_personal_dialogue_proof(const BornPersonalDialogueProof&);
Outcome<BornPersonalDialogueProof> decode_born_personal_dialogue_proof(const BoundaryValue&);
Status visit_born_personal_dialogue_proof(CodecVisitor&,BornPersonalDialogueProof&);
}
