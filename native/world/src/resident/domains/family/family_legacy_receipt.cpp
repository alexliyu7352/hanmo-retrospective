// 缓存不保存第二条控制链；三个命令的结果必须逐字段等于原永久事实。
#include "family_legacy_receipt.hpp"
namespace hanmo::resident {
Status validate_family_legacy_receipt(const FamilyLegacyState& state,const StableId& kind,const StableId& actor,Integer tick,const FamilyEffect& effect){
    if(effect.success!=std::optional<bool>{true}||effect.already_completed||effect.code||effect.actor_id||!effect.fact_id||!effect.heir_actor_id||!effect.player_actor_id)return common_failure("InvalidFamilyLegacyReceipt");
    const auto& id=*effect.fact_id;const LegacyControlDecisionSource* source=nullptr;StableId player;Integer recorded_tick=-1;
    if(kind=="family_legacy_accept"){const auto* fact=state.consents.find(id);if(fact){source=&fact->source;recorded_tick=source->tick;player=source->predecessor_actor_id;}}
    else if(kind=="family_legacy_revoke"){const auto* fact=state.revocations.find(id);if(fact){source=&fact->source;recorded_tick=source->tick;player=source->predecessor_actor_id;}}
    else if(kind=="family_legacy_activate"){const auto* fact=state.activations.find(id);const auto* consent=fact?state.consents.find(fact->consent_fact_id):nullptr;if(consent){source=&consent->source;recorded_tick=fact->tick;player=source->successor_actor_id;}}
    if(!source||actor!=source->predecessor_actor_id||tick!=recorded_tick||*effect.heir_actor_id!=source->successor_actor_id||*effect.player_actor_id!=player)return common_failure("FamilyLegacyReceiptSourceMismatch",id);
    return common_success();
}
}
