// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
#include "social_window_codec.hpp"
#include "social_window_codec_fields.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/storage/common_checkpoint.hpp"
namespace hanmo::resident {namespace {
template<class T,class Emit>Outcome<BoundaryValue> encode_record(const T& value,Emit emit){
 BoundaryValue output;BoundaryProjectionWriter writer(output);
 auto valid=writer.object("",[&](ProjectionWriter& child){return emit(child,value);});
 if(valid.ok)valid=writer.finish();return {valid.ok,std::move(output),valid.error};
}
template<class T,class Fields>Outcome<T> decode_record(const BoundaryValue& input,Fields fields){
 T value;ActorFoundationJsonVisitor reader(input);auto valid=fields(reader,value);
 if(valid.ok)valid=reader.finish();return {valid.ok,std::move(value),valid.error};
}
template<class T,class Fields>Status emit_record(ProjectionWriter& writer,const T& value,Fields fields){
 ActorFoundationProjectionArchive archive(writer);
 return fields(archive,const_cast<T&>(value));
}
}
Status visit_gift_benefit_receipt(CodecVisitor& c,GiftBenefitReceipt& r){return social_window_codec::receipt(c,r);}
Status visit_gift_window_refs(CodecVisitor& c,GiftWindowRefs& r){return social_window_codec::window(c,r);}
Status visit_meeting_benefit_key(CodecVisitor& c,MeetingBenefitKey& r){return social_window_codec::meeting(c,r);}
Status visit_topic_benefit_key(CodecVisitor& c,TopicBenefitKey& r){return social_window_codec::topic(c,r);}
Status emit_gift_benefit_receipt_fields(ProjectionWriter& c,const GiftBenefitReceipt& r){return emit_record(c,r,[](auto& x,auto& v){return social_window_codec::receipt(x,v);});}
Status emit_gift_window_refs_fields(ProjectionWriter& c,const GiftWindowRefs& r){return emit_record(c,r,[](auto& x,auto& v){return social_window_codec::window(x,v);});}
Status emit_meeting_benefit_key_fields(ProjectionWriter& c,const MeetingBenefitKey& r){return emit_record(c,r,[](auto& x,auto& v){return social_window_codec::meeting(x,v);});}
Status emit_topic_benefit_key_fields(ProjectionWriter& c,const TopicBenefitKey& r){return emit_record(c,r,[](auto& x,auto& v){return social_window_codec::topic(x,v);});}
Outcome<BoundaryValue> encode_gift_benefit_receipt(const GiftBenefitReceipt& r){return encode_record(r,emit_gift_benefit_receipt_fields);}
Outcome<BoundaryValue> encode_gift_window_refs(const GiftWindowRefs& r){return encode_record(r,emit_gift_window_refs_fields);}
Outcome<BoundaryValue> encode_meeting_benefit_key(const MeetingBenefitKey& r){return encode_record(r,emit_meeting_benefit_key_fields);}
Outcome<BoundaryValue> encode_topic_benefit_key(const TopicBenefitKey& r){return encode_record(r,emit_topic_benefit_key_fields);}
Outcome<GiftBenefitReceipt> decode_gift_benefit_receipt(const BoundaryValue& r){return decode_record<GiftBenefitReceipt>(r,[](auto& x,auto& v){return social_window_codec::receipt(x,v);});}
Outcome<GiftWindowRefs> decode_gift_window_refs(const BoundaryValue& r){return decode_record<GiftWindowRefs>(r,[](auto& x,auto& v){return social_window_codec::window(x,v);});}
Outcome<MeetingBenefitKey> decode_meeting_benefit_key(const BoundaryValue& r){return decode_record<MeetingBenefitKey>(r,[](auto& x,auto& v){return social_window_codec::meeting(x,v);});}
Outcome<TopicBenefitKey> decode_topic_benefit_key(const BoundaryValue& r){return decode_record<TopicBenefitKey>(r,[](auto& x,auto& v){return social_window_codec::topic(x,v);});}
}
