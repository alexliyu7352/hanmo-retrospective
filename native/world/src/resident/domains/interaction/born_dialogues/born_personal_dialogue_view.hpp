// 只列本人在场可见的成年亲生子代模板；不显示未出生、远方、未知来源或替人答允。
#pragma once
#include "born_personal_dialogue_boundary.hpp"
namespace hanmo::resident {
Outcome<BoundaryValue> query_born_personal_dialogues(const CommonStore&,const InteractionState&,const BornPersonalDialogueState&,const InteractionCatalog&,const BornPersonalDialogueCatalog&,const InteractionAccess&,const BornPersonalDialogueSources&,Integer cursor=0);
}
