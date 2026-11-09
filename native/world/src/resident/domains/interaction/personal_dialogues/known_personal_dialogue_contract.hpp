// 两个真实成年人的具名私人SAY入口；人物出处不等于亲属、认可、本人同意或控制资格。
#pragma once
#include "known_personal_dialogue_state.hpp"
#include "resident/domains/family/born_actor_source.hpp"
#include "resident/domains/interaction/dialogue_provider_source.hpp"
namespace hanmo::resident {
class FamilyContinuationPeopleRead4;
// 作者端点只有两个显式来源；installed_actor的source_id是原人物，born_prototype是原出生原型。
struct KnownPersonalDialogueSelector {
  StableId kind, source_id;
};
struct KnownPersonalDialogueTemplate {
  StableId id, dialogue_id;
  KnownPersonalDialogueSelector listener, provider;
};
struct KnownPersonalDialogueCatalog {
  Integer version = 1;
  std::vector<KnownPersonalDialogueTemplate> templates;
};
struct KnownPersonalDialogueProof {
  StableId opening_fact_id, template_id;
  AuthoredProviderSourceProof declaration;
  KnownPersonalActorOrigin listener_origin, provider_origin;
};
class KnownPersonalDialogueRead {
public:
  virtual ~KnownPersonalDialogueRead() = default;
  virtual Outcome<KnownPersonalDialogueProof>
  known_personal_dialogue(const StableId &listener, const StableId &provider,
                          const StableId &template_id) const = 0;
  // 消费方先核自己真实业务头及payload.tick；只重验这份已保存的opening，不能另选会面补史。
  virtual Status
  known_personal_dialogue_before_fact(const KnownPersonalDialogueProof &,
                                      Integer tick,
                                      const StableId &actual_fact) const = 0;
};
// 完整同候选Family/Creation/Birth/Lineage/Life先验；此处不回调亲缘认可/控制许可总验证。
struct KnownPersonalDialogueSources {
  BornActorSourceRead born;
  BornActorSourceBeforeRead born_before;
  std::function<Outcome<ActorPresenceRead>(const StableId &)> presence;
  std::function<Outcome<ActorPresenceRead>(const StableId &, Integer,
                                           const StableId &)>
      presence_before;
  std::function<Outcome<StableId>(Integer, const StableId &)> player_before;
  // Complete4 显式借用同根年龄源；旧 profile 保空并保持原算法与存档。
  const FamilyContinuationPeopleRead4 *complete_people = nullptr;
};
BoundaryValue
encode_known_personal_dialogue_proof(const KnownPersonalDialogueProof &);
Outcome<KnownPersonalDialogueProof>
decode_known_personal_dialogue_proof(const BoundaryValue &);
Status visit_known_personal_dialogue_proof(CodecVisitor &,
                                           KnownPersonalDialogueProof &);
} // namespace hanmo::resident
