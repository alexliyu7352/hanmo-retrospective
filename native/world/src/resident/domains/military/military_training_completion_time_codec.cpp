#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "military_training_completion_time_codec.hpp"
#include "military_state_codec_archive.hpp"
#include "resident/kernel/exact_struct_codec.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
namespace hanmo::resident {using namespace movement_detail;namespace {
// Only decode/projection staging. The sole persisted field is on C06's row.
struct Staging {OrderedTable<SourceRef> training_completions;};
struct Keys {
 template<class T>static constexpr bool owns=std::is_same_v<T,SourceRef>;
 bool operator()(const StableId& key,const SourceRef& source)const{return !key.empty()&&source.domain=="military"&&source.kind=="military_training_completion_applied"&&!source.id.empty()&&source.id!=key;}
};
template<class A>Status fields(A& archive,Staging& staging,Integer selected){
 if(selected!=1)return archive.reject("MilitaryTrainingCompletionTimeSelectedSchemaRequired");Integer schema=selected;archive.field("schema",schema);if(!archive.status().ok)return archive.status();if(schema!=selected)return archive.reject("MilitaryTrainingCompletionTimeSelectedSchemaMismatch");
 military_state_codec::RootArchive<A,Keys> root{archive};root.table("training_completions",staging.training_completions,[](auto& child,auto& row){return exact_fields::source(child,row);});return root.status();
}
void empty_targets(const MilitaryActionTimeState& state){for(const auto& item:state.dynamic_training_facts.rows())if(item.second.completion_application_source)fail("MilitaryTrainingCompletionAlreadyAttached",item.first);}
void collect(const MilitaryActionTimeState& state,Staging& staging){
 take(validate_military_training_completion_time_layout(state,1));
 for(const auto& item:state.dynamic_training_facts.rows())if(item.second.completion_application_source&&!staging.training_completions.insert(item.first,*item.second.completion_application_source))fail("MilitaryTrainingCompletionDuplicate",item.first);
}
CheckpointMemoryCharge reserve_row(const DynamicMilitaryTrainingFact2& row,const SourceRef& source){return CheckpointMemoryCharge(0);}
struct PreparedRow {
 CheckpointMemoryCharge copied_fields;DynamicMilitaryTrainingFact2 row;
 PreparedRow(const DynamicMilitaryTrainingFact2& original,const SourceRef& source):copied_fields(reserve_row(original,source)),row(original){row.completion_application_source.emplace(source);}
};
void attach(MilitaryActionTimeState& state,const Staging& staging){
 std::size_t completed=0;for(const auto& item:state.dynamic_training_facts.rows())if(item.second.phase==DynamicMilitaryTrainingPhase::completed)++completed;
 if(completed!=staging.training_completions.size())fail("MilitaryTrainingCompletionCoverageMismatch");
 for(const auto& item:staging.training_completions.rows()){const auto* row=state.dynamic_training_facts.find(item.first);if(!row||row->id!=item.first||row->rule_version!=2||row->phase!=DynamicMilitaryTrainingPhase::completed||!row->completed_at||!row->terminal_source_ref||row->completion_application_source||!Keys{}(item.first,item.second))fail("MilitaryTrainingCompletionOrphan",item.first);}
 for(const auto& item:staging.training_completions.rows()){PreparedRow candidate(*state.dynamic_training_facts.find(item.first),item.second);state.dynamic_training_facts.set(item.first,candidate.row);}
 take(validate_military_training_completion_time_layout(state,1));
}
}
Status visit_military_training_completion_time(CodecVisitor& c,MilitaryActionTimeState& state,Integer selected){return status([&]{try{
 if(selected!=1)fail("MilitaryTrainingCompletionTimeSelectedSchemaRequired");if(c.reading())empty_targets(state);Staging staging;if(!c.reading())collect(state,staging);take(fields(c,staging,selected));if(c.reading())attach(state,staging);
}catch(const exact_fields::CodecFailure& e){throw Failure{e.error};}});}
Status decode_military_training_completion_time(const BoundaryValue& raw,MilitaryActionTimeState& state,Integer selected){return status([&]{try{
 if(selected!=1)fail("MilitaryTrainingCompletionTimeSelectedSchemaRequired");if(!ScopedCheckpointJsonDecode::current())fail("CheckpointJsonContextRequired");empty_targets(state);Staging staging;ActorFoundationJsonVisitor c(raw);take(c.ordered_fields());take(fields(c,staging,selected));take(c.finish());attach(state,staging);
}catch(const exact_fields::CodecFailure& e){throw Failure{e.error};}});}
Status emit_military_training_completion_time_fields(ProjectionWriter& sink,const MilitaryActionTimeState& state,Integer selected){return status([&]{try{
 if(selected!=1)fail("MilitaryTrainingCompletionTimeSelectedSchemaRequired");Staging staging;collect(state,staging);ActorFoundationProjectionArchive c(sink);take(fields(c,staging,selected));
}catch(const exact_fields::CodecFailure& e){throw Failure{e.error};}});}
Status emit_military_training_completion_time(ProjectionWriter& sink,std::string_view name,const MilitaryActionTimeState& state,Integer selected){return sink.object(name,[&](auto& fields){return emit_military_training_completion_time_fields(fields,state,selected);});}
Outcome<BoundaryValue> encode_military_training_completion_time(const MilitaryActionTimeState& state,Integer selected){return guard<BoundaryValue>([&]{BoundaryValue out;BoundaryProjectionWriter sink(out);take(emit_military_training_completion_time(sink,"",state,selected));take(sink.finish());return out;});}
}
