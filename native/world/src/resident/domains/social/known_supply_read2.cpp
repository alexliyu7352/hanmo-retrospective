#include "known_supply_read2.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
namespace hanmo::resident {namespace {
bool equal_observation(const SupplyObservation&a,const SupplyObservation&b){return a.source_key==b.source_key&&a.organization_id==b.organization_id&&a.site_id==b.site_id&&a.inventory_id==b.inventory_id&&a.item_id==b.item_id&&a.source_fact_id==b.source_fact_id&&a.target_amount==b.target_amount&&a.physical_amount==b.physical_amount&&a.source_revision==b.source_revision;}
}
Outcome<std::optional<KnownSupplyObservation2>> read_known_supply_observation2(const CommonStore&common,const SocialState&social,const ExactResultTimeRead&source,const ExactResultReadStamp&stamp,const SubjectRef&subject,const StableId&inventory,const StableId&site,const StableId&item,const WorldMoment&actual){
 using Result=Outcome<std::optional<KnownSupplyObservation2>>;
 if(subject.id.empty()||inventory.empty()||site.empty()||item.empty())return {false,{},common_failure("KnownSupplyReadIdentityMissing").error};
 if(!social.dynamic||!social.dynamic->time2)return {false,{},common_failure("ExactSocialKnowledgeRequired").error};
 const auto*known=find_social_knowledge_table(*social.dynamic,subject);if(!known)return {true,{},{}};
 const Report*selected_report=nullptr;const SupplyObservation*selected_observation=nullptr;const DynamicFactViewV2*selected_original=nullptr;
 for(const auto&row:known->rows()){
  const auto*report=social.dynamic->reports.find(row.second.report_id);if(!report||row.first!=report->id||!same_social_subject(row.second.receiver,subject))return {false,{},common_failure("KnownSupplyReportMissing",row.first).error};
  const auto*observation=report->observation.supply?&*report->observation.supply:nullptr;
  if(!observation||observation->inventory_id!=inventory||observation->site_id!=site||observation->item_id!=item)continue;
  auto receipt=read_knowledge_receipt_view_v2(common,social,source,subject,report->id,stamp);if(!receipt.ok)return {false,{},receipt.error};if(!receipt.value)return {false,{},common_failure("KnownSupplyReceiptMissing",report->id).error};
  auto learned=compare_world_moments(receipt.value->learned_at,actual);if(!learned.ok)return {false,{},learned.error};if(learned.value>0)continue;
  auto original=source.fact_v2_borrowed(report->fact_id,stamp);if(!original.ok)return {false,{},original.error};if(!original.value||!original.value->supply||report->fact_id!=observation->source_fact_id||report->observation.id!=report->fact_id||!equal_observation(*original.value->supply,*observation))return {false,{},common_failure("KnownSupplyObservationSourceMismatch",report->id).error};
  auto observed=compare_world_moments(original.value->occurred_at,actual);if(!observed.ok)return {false,{},observed.error};if(observed.value>0)return {false,{},common_failure("KnownSupplyObservationFromFuture",report->id).error};
  // Retain the original Calendar validity bucket; it is not a new exact deadline.
  if(report->valid_until_tick&&actual.half_day_tick>*report->valid_until_tick)continue;
  if(selected_report){auto newer=compare_world_moments(original.value->occurred_at,selected_original->occurred_at);if(!newer.ok)return {false,{},newer.error};if(newer.value<0||(newer.value==0&&observation->source_revision<=selected_observation->source_revision))continue;}
  selected_report=report;selected_observation=observation;selected_original=original.value;
 }
 if(!selected_report)return Result{true,std::nullopt,{}};
 // Only the final winning observation is materialized. Earlier candidates use
 // the original KnownReport and applied-time view without deep string copies.
 auto selected_receipt=read_knowledge_receipt_view_v2(common,social,source,subject,selected_report->id,stamp);if(!selected_receipt.ok)return {false,{},selected_receipt.error};if(!selected_receipt.value||!selected_receipt.value->original)return {false,{},common_failure("KnownSupplyReceiptMissing",selected_report->id).error};
 const auto&original_receipt=*selected_receipt.value->original;KnownSupplyObservation2 selected;(selected.memory).grow(0);
 selected.report=selected_report;selected.observation=selected_observation;selected.receipt={original_receipt.receiver,original_receipt.report_id,original_receipt.source_message_id,original_receipt.learned_tick,original_receipt.source_subject,std::move(selected_receipt.value->learned_at)};selected.observed_at=selected_original->occurred_at;
 return Result{true,std::optional<KnownSupplyObservation2>{std::move(selected)},{}};
}
}
