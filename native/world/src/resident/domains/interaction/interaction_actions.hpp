// 已有发现、同行和对白确认的领域入口；中央只协调跨域请求，不解释业务条件。
#pragma once
#include "interaction_codec.hpp"
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {
Outcome<InteractionQuote> preview_interaction_extra(const CommonRead&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const StableId&,const InteractionCommand&);
Outcome<InteractionEffect> apply_interaction_extra(CommonStore&,InteractionState&,const InteractionCatalog&,const InteractionAccess&,const StableId&,const InteractionCommand&,const InteractionQuote&);
bool is_interaction_extra(const InteractionCommand&);
bool same_dialogue_command(const DialogueCommand&,const DialogueCommand&);
Outcome<DialogueCommand> pending_dialogue_request(const InteractionState&,const StableId&);
Status resolve_dialogue(InteractionState&,const InteractionCatalog&,const StableId&,const DialogueOutcome&);
Status finish_dialogue_confirmation(CommonWrite&,InteractionState&,const InteractionCatalog&,const InteractionAccess&,const StableId&,const StableId& nested_kind,const std::optional<StableId>& action_id={});
Status remember_interaction_offer(InteractionState&,const StableId&,const StableId&,const KnowledgeSource&,Integer);
BoundaryValue encode_interaction_extra_quote(const StableId&,const InteractionQuote&);
BoundaryValue encode_interaction_extra_effect(const InteractionEffect&);
Outcome<BoundaryValue> query_interaction_topics(const CommonRead&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const StableId&,const StableId& place_id);
}
