// 人物模板权源只匹配同一个Installed本人或同一真实Birth，绝不读取/声明旁支关系。
#pragma once
#include "known_personal_dialogue_contract.hpp"
namespace hanmo::resident {
Outcome<KnownPersonalActorOrigin> read_known_personal_actor_origin(const CommonStore&,const CommonCatalog&,const KnownPersonalDialogueSources&,const KnownPersonalDialogueSelector&,const StableId& actor);
Outcome<KnownPersonalActorOrigin> read_known_personal_actor_origin_before_fact(const CommonStore&,const CommonCatalog&,const KnownPersonalDialogueSources&,const KnownPersonalDialogueSelector&,const StableId& actor,Integer tick,const StableId& actual_fact);
bool same_known_personal_actor_origin(const KnownPersonalActorOrigin&,const KnownPersonalActorOrigin&);
}
