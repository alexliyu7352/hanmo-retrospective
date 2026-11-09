// Original P24.4 / V12.2 records, owned once by social_evaluations/1. These
// are individual record types, not a second SocialState or a partial section.
#pragma once
#include "social_window_rules.hpp"
#include "resident/domains/actor_foundation/actor_foundation_refs.hpp"
#include "resident/kernel/world_timeline.hpp"
namespace hanmo::resident {
struct GiftBenefitReceipt {
 MomentOwnerSource source_gift_transfer_ref;
 StableId giver,receiver,gift_class_id;
 Integer accepted_tick=0,window_ordinal=0,factor_bp=0,base_affinity=0;
 Integer applied_affinity=0,applied_trust=0;
 ActorFoundationRuleRef rule_version;
 Integer gift_unit_ordinal=0;
};
struct GiftWindowRefs {
 StableId giver,receiver,gift_class_id;
 std::vector<StableId> receipt_refs;
};
struct MeetingBenefitKey {
 StableId from_actor,to_actor;
 Integer game_day_ordinal=0;
 MomentOwnerSource actual_meeting_result_ref;
};
struct TopicBenefitKey {
 StableId from_actor,to_actor,topic_definition_id;
 MomentOwnerSource actual_topic_result_ref;
};
Status validate_gift_benefit_receipt_shape(const GiftBenefitReceipt&);
Status validate_gift_window_refs_shape(const GiftWindowRefs&);
Status validate_meeting_benefit_key_shape(const MeetingBenefitKey&);
Status validate_topic_benefit_key_shape(const TopicBenefitKey&);
// once is the original accepted transfer + actual unit ordinal + immutable
// class. No equality of display names, inventory lots, or Calendar day is used.
bool same_gift_benefit_once(const GiftBenefitReceipt&,const GiftBenefitReceipt&)noexcept;
}
