#include "resident/domains/growth/growth_model.hpp"
#include "growth_command_result_retention.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {namespace {
Status emit(GrowthCommandResultSink& sink,GrowthCommandResultSlot slot,const StableId& consumer,const StableId& actor,const StableId& kind,Integer seq,const StableId& result,std::optional<Integer> revision={},Integer ordinal=0){
 if(consumer.empty()||actor.empty()||kind.empty()||seq<1||result.empty()||(revision&&*revision<1))return common_failure("GrowthOriginalCommandReferenceUnbound",consumer);
 return sink.actual_reference({slot,consumer,actor,kind,result,seq,ordinal,revision});
}
}
Status visit_growth_command_result_references(const GrowthState& growth,const GrowthTacticLessonState* tactics,const GrowthRecoveryState* recovery,GrowthCommandResultSink& sink){
 for(const auto& row:growth.fixed_service_boundaries.rows()){
  const auto& boundary=row.second;const LifeActualCommand* command=std::get_if<LifeActualCommand>(&boundary.cause);
  if(const auto* terminal=std::get_if<LifeOriginalTerminalApplication>(&boundary.cause))if(const auto* stop=std::get_if<LifeActualStopTrigger>(&terminal->trigger))command=std::get_if<LifeActualCommand>(&stop->source);
  if(command){const auto& value=command->command_binding;auto valid=emit(sink,GrowthCommandResultSlot::fixed_command,row.first,command->actor_id,value.command_kind,value.request_seq,value.result_id);if(!valid.ok)return valid;}
 }
 for(const auto& row:growth.facts.rows())if(const auto* study=std::get_if<StudyFact>(&row.second)){
  for(std::size_t n=0;n<study->lessons.size();++n){const auto& lesson=study->lessons[n];if(lesson.segment&&lesson.segment->adjustment&&lesson.segment->adjustment->exact_binding){const auto& value=*lesson.segment->adjustment->exact_binding;auto valid=emit(sink,GrowthCommandResultSlot::study_adjustment,row.first,value.actor_id,value.command_kind,value.request_seq,value.result_id,value.revision,static_cast<Integer>(n+1));if(!valid.ok)return valid;}}
  if(study->study_service_terminal){Integer ordinal=0;for(const auto& release:study->study_service_terminal->reservation_release_results){++ordinal;const auto& value=release.original_release_commit_ref;auto valid=emit(sink,GrowthCommandResultSlot::study_reservation_release,row.first,value.actor_id,value.kind,value.request_seq,value.result_id,value.revision,ordinal);if(!valid.ok)return valid;}}
 }
 if(tactics){for(const auto& row:tactics->starts.rows()){const auto& value=row.second.commit;auto valid=emit(sink,GrowthCommandResultSlot::tactic_start,row.first,value.actor_id,value.kind,value.request_seq,value.result_id,value.revision);if(!valid.ok)return valid;}for(const auto& row:tactics->closures.rows()){const auto& value=row.second.commit;auto valid=emit(sink,GrowthCommandResultSlot::tactic_closure,row.first,value.actor_id,value.kind,value.request_seq,value.result_id,value.revision);if(!valid.ok)return valid;}}
 if(recovery)for(const auto& row:recovery->closures.rows()){const auto& value=row.second;auto valid=emit(sink,GrowthCommandResultSlot::recovery_closure,row.first,value.commit_actor_id,value.commit_kind,value.commit_request_seq,value.commit_result_id,value.commit_revision);if(!valid.ok)return valid;}
 return common_success();
}
Status visit_growth_adjustment_fact_prefix_references(const CommonStore& common,const GrowthState& growth,const GrowthCatalog& catalog,GrowthAdjustmentFactPrefixSink& sink){
 const auto& heads=common.state().facts.order();
 for(const auto& row:growth.facts.rows())if(const auto* study=std::get_if<StudyFact>(&row.second))for(std::size_t n=0;n<study->lessons.size();++n){
  const auto& lesson=study->lessons[n];if(!lesson.segment||!lesson.segment->adjustment||!lesson.segment->adjustment->exact_binding)continue;const auto& binding=*lesson.segment->adjustment->exact_binding;
  if(!catalog.complete_exact_command_cuts){if(binding.original_fact_prefix)return common_failure("StudyAdjustmentFactPrefixPhysicalSelectionRequired",row.first);continue;}
  if(!binding.original_fact_prefix)return common_failure("StudyAdjustmentFactPrefixRequired",row.first);const auto& cut=*binding.original_fact_prefix;
  if(cut.original_fact_prefix_size<0||static_cast<std::size_t>(cut.original_fact_prefix_size)>heads.size()||(cut.original_fact_prefix_size==0)!=!cut.last_fact_id||(cut.last_fact_id&&heads[static_cast<std::size_t>(cut.original_fact_prefix_size-1)]!=*cut.last_fact_id))return common_failure("StudyAdjustmentFactPrefixMismatch",row.first);
  for(std::size_t i=0;i<static_cast<std::size_t>(cut.original_fact_prefix_size);++i){if(!common.state().facts.find(heads[i]))return common_failure("StudyAdjustmentOriginalPrefixHeadMissing",heads[i]);auto valid=sink.actual_prefix_fact(row.first,static_cast<Integer>(n+1),heads[i]);if(!valid.ok)return valid;}
 }
 return common_success();
}
}
