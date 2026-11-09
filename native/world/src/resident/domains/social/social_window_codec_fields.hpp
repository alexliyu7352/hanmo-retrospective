// Exact P24.4 prefix and V12.2 unit/source tails. This table is shared by the
// eventual whole seven-root section and every binary/JSON/projection record.
#pragma once
#include "social_window_model.hpp"
#include "resident/domains/actor_foundation/character_origin_codec_detail.hpp"
#include "resident/domains/family/birth_due_execution_codec_fields.hpp"
namespace hanmo::resident::social_window_codec {
template<class C>Status receipt(C& c,GiftBenefitReceipt& r){
 c.object("source_gift_transfer_ref",[&](auto& x){return birth_due_codec::moment_source(x,r.source_gift_transfer_ref);});
 c.field("giver",r.giver);c.field("receiver",r.receiver);c.field("gift_class_id",r.gift_class_id);
 c.field("accepted_tick",r.accepted_tick);c.field("window_ordinal",r.window_ordinal);
 c.field("factor_bp",r.factor_bp);c.field("base_affinity",r.base_affinity);
 c.field("applied_affinity",r.applied_affinity);c.field("applied_trust",r.applied_trust);
 c.object("rule_version",[&](auto& x){return character_origin_codec::rule(x,r.rule_version);});
 c.field("gift_unit_ordinal",r.gift_unit_ordinal);
 if(!c.status().ok)return c.status();auto valid=validate_gift_benefit_receipt_shape(r);
 return valid.ok?c.status():c.reject(valid.error.original.code);
}
template<class C>Status window(C& c,GiftWindowRefs& r){
 c.field("giver",r.giver);c.field("receiver",r.receiver);c.field("gift_class_id",r.gift_class_id);
 c.sequence("receipt_refs",r.receipt_refs,[](auto& x,auto& id){return x.field("value",id);});
 if(!c.status().ok)return c.status();auto valid=validate_gift_window_refs_shape(r);
 return valid.ok?c.status():c.reject(valid.error.original.code);
}
template<class C>Status meeting(C& c,MeetingBenefitKey& r){
 c.field("from_actor",r.from_actor);c.field("to_actor",r.to_actor);
 c.field("game_day_ordinal",r.game_day_ordinal);
 c.object("actual_meeting_result_ref",[&](auto& x){return birth_due_codec::moment_source(x,r.actual_meeting_result_ref);});
 if(!c.status().ok)return c.status();auto valid=validate_meeting_benefit_key_shape(r);
 return valid.ok?c.status():c.reject(valid.error.original.code);
}
template<class C>Status topic(C& c,TopicBenefitKey& r){
 c.field("from_actor",r.from_actor);c.field("to_actor",r.to_actor);
 c.field("topic_definition_id",r.topic_definition_id);
 c.object("actual_topic_result_ref",[&](auto& x){return birth_due_codec::moment_source(x,r.actual_topic_result_ref);});
 if(!c.status().ok)return c.status();auto valid=validate_topic_benefit_key_shape(r);
 return valid.ok?c.status():c.reject(valid.error.original.code);
}
}
