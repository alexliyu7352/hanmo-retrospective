#include "social_window_model.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {namespace {
bool source_shape(const MomentOwnerSource& source){return
 !source.source_ref.domain.empty()&&!source.source_ref.kind.empty()&&
 !source.source_ref.id.empty()&&source.source_schema>0;}
bool rule_shape(const ActorFoundationRuleRef& rule){return
 !rule.id.empty()&&rule.version>0&&!rule.catalog_digest.empty();}
Status invalid(const char* code){return common_failure(code,{},ErrorKind::internal);}
}
Status validate_gift_benefit_receipt_shape(const GiftBenefitReceipt& r){
 if(!source_shape(r.source_gift_transfer_ref)||r.giver.empty()||r.receiver.empty()||
    r.gift_class_id.empty()||r.accepted_tick<0||r.window_ordinal<1||
    r.gift_unit_ordinal<0||!rule_shape(r.rule_version)||
    (r.base_affinity!=6&&r.base_affinity!=10))return invalid("InvalidGiftBenefitReceipt");
 const auto kind=r.base_affinity==6?OriginalAcceptedGiftClass::ordinary_agreeable:
                                    OriginalAcceptedGiftClass::precious_agreeable;
 auto maximum=original_social_gift_benefit(kind,r.window_ordinal-1,1);
 if(!maximum.ok)return {false,{},maximum.error};
 if(r.factor_bp!=maximum.value.factor_bp||r.applied_affinity!=maximum.value.applied_affinity||
    r.applied_trust<0||r.applied_trust>maximum.value.applied_trust)
  return invalid("GiftBenefitOriginalRuleMismatch");
 return common_success();
}
Status validate_gift_window_refs_shape(const GiftWindowRefs& r){
 if(r.giver.empty()||r.receiver.empty()||r.gift_class_id.empty())
  return invalid("InvalidGiftWindowRefs");
 for(std::size_t i=0;i<r.receipt_refs.size();++i){
  if(r.receipt_refs[i].empty())return invalid("InvalidGiftWindowReceiptRef");
  for(std::size_t j=0;j<i;++j)if(r.receipt_refs[i]==r.receipt_refs[j])
   return invalid("DuplicateGiftWindowReceiptRef");
 }
 return common_success();
}
Status validate_meeting_benefit_key_shape(const MeetingBenefitKey& r){
 return r.from_actor.empty()||r.to_actor.empty()||r.game_day_ordinal<0||
        !source_shape(r.actual_meeting_result_ref)?invalid("InvalidMeetingBenefitKey"):
                                                 common_success();
}
Status validate_topic_benefit_key_shape(const TopicBenefitKey& r){
 return r.from_actor.empty()||r.to_actor.empty()||r.topic_definition_id.empty()||
        !source_shape(r.actual_topic_result_ref)?invalid("InvalidTopicBenefitKey"):
                                               common_success();
}
bool same_gift_benefit_once(const GiftBenefitReceipt& a,const GiftBenefitReceipt& b)noexcept{
 const auto& x=a.source_gift_transfer_ref;const auto& y=b.source_gift_transfer_ref;
 return x.source_ref.domain==y.source_ref.domain&&x.source_ref.kind==y.source_ref.kind&&
  x.source_ref.id==y.source_ref.id&&x.source_schema==y.source_schema&&
  a.gift_unit_ordinal==b.gift_unit_ordinal&&a.gift_class_id==b.gift_class_id;
}
}
