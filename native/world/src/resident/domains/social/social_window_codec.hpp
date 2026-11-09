#pragma once
#include "social_window_model.hpp"
namespace hanmo::resident {
struct CodecVisitor;class ProjectionWriter;
Status visit_gift_benefit_receipt(CodecVisitor&,GiftBenefitReceipt&);
Status visit_gift_window_refs(CodecVisitor&,GiftWindowRefs&);
Status visit_meeting_benefit_key(CodecVisitor&,MeetingBenefitKey&);
Status visit_topic_benefit_key(CodecVisitor&,TopicBenefitKey&);
Status emit_gift_benefit_receipt_fields(ProjectionWriter&,const GiftBenefitReceipt&);
Status emit_gift_window_refs_fields(ProjectionWriter&,const GiftWindowRefs&);
Status emit_meeting_benefit_key_fields(ProjectionWriter&,const MeetingBenefitKey&);
Status emit_topic_benefit_key_fields(ProjectionWriter&,const TopicBenefitKey&);
Outcome<BoundaryValue> encode_gift_benefit_receipt(const GiftBenefitReceipt&);
Outcome<BoundaryValue> encode_gift_window_refs(const GiftWindowRefs&);
Outcome<BoundaryValue> encode_meeting_benefit_key(const MeetingBenefitKey&);
Outcome<BoundaryValue> encode_topic_benefit_key(const TopicBenefitKey&);
Outcome<GiftBenefitReceipt> decode_gift_benefit_receipt(const BoundaryValue&);
Outcome<GiftWindowRefs> decode_gift_window_refs(const BoundaryValue&);
Outcome<MeetingBenefitKey> decode_meeting_benefit_key(const BoundaryValue&);
Outcome<TopicBenefitKey> decode_topic_benefit_key(const BoundaryValue&);
}
