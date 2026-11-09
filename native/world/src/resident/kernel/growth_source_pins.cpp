#include "growth_source_pins.hpp"
#include "common_command_sources.hpp"
#include "world_bundle.hpp"
#include "resident/domains/growth/growth_command_result_retention.hpp"
#include <set>
#include <charconv>
namespace hanmo::resident {namespace {
const char* slot_name(GrowthCommandResultSlot slot){
 switch(slot){case GrowthCommandResultSlot::fixed_command:return "fixed_command";
 case GrowthCommandResultSlot::study_adjustment:return "study_adjustment";
 case GrowthCommandResultSlot::study_reservation_release:return "study_reservation_release";
 case GrowthCommandResultSlot::tactic_start:return "tactic_start";
 case GrowthCommandResultSlot::tactic_closure:return "tactic_closure";
 case GrowthCommandResultSlot::recovery_closure:return "recovery_closure";}
 return nullptr;
}
SourcePinConsumerId cell_key(std::string_view original,Integer ordinal,std::string_view child={}){
 char size[32],number[32],child_size[32];
 const auto a=std::to_chars(size,size+sizeof size,original.size());
 const auto b=std::to_chars(number,number+sizeof number,ordinal);
 const auto c=std::to_chars(child_size,child_size+sizeof child_size,child.size());
 if(a.ec!=std::errc{}||b.ec!=std::errc{}||c.ec!=std::errc{})throw std::length_error("GrowthSourceConsumerIdOverflow");
 return SourcePinConsumerId{{size,static_cast<std::size_t>(a.ptr-size)},":",original,":",{number,static_cast<std::size_t>(b.ptr-number)},":",{child_size,static_cast<std::size_t>(c.ptr-child_size)},":",child};
}
bool same(const SourcePinBuildView& a,const SourcePin& b){return a.consumer_domain==b.consumer.domain&&a.consumer_kind==b.consumer.kind&&a.consumer_id==b.consumer.id&&a.consumer_slot==b.consumer.slot&&a.source_domain==b.source.domain&&a.source_kind==b.source.kind&&a.source_id==b.source.id&&a.reason==b.reason;}
Status selection(const WorldBundle& w){return w.catalog&&w.catalog->growth&&w.catalog->exact_time&&exact_complete_selected(*w.catalog->exact_time)?common_success():common_failure("CompleteGrowthSourceSelectionRequired");}
class Commands final:public GrowthCommandResultSink {
 const WorldBundle& w_;GrowthCommandResultSlot slot_;SourcePinSink* output_;const SourcePin* requested_;
public:
 bool found=false;
 Commands(const WorldBundle& w,GrowthCommandResultSlot slot,SourcePinSink* out,const SourcePin* requested):w_(w),slot_(slot),output_(out),requested_(requested){}
 Status actual_reference(const GrowthCommandResultReference& ref)override{
  if(ref.slot!=slot_)return common_success();
  if(ref.consumer_ordinal<0)return common_failure("InvalidGrowthSourceConsumerOrdinal");
  auto actual=read_common_command_source(w_.common,std::string_view(ref.result_id));if(!actual.ok)return {false,{},std::move(actual.error)};
  if(actual.value.history->actor_id!=ref.actor_id||actual.value.result->kind!=ref.command_kind||actual.value.request_seq!=ref.request_seq||(ref.committed_revision&&actual.value.result->revision!=*ref.committed_revision))return common_failure("PinnedGrowthCommandIdentityMismatch",StableId(ref.result_id));
  const auto id=cell_key(ref.consumer_id,ref.consumer_ordinal);
  const SourcePinBuildView pin{"growth",slot_name(slot_),id.value(),"command_result","common","command_result",ref.result_id,SourcePinReason::unique_outcome};
  if(requested_&&same(pin,*requested_))found=true;return output_?output_->append(pin):common_success();
 }
};
Status commands(const WorldBundle& w,Commands& sink){auto valid=selection(w);if(!valid.ok)return valid;return visit_growth_command_result_references(w.growth,w.growth_tactic_lessons_enabled()?&w.growth_tactic_lessons:nullptr,w.personal_recovery_enabled()?&w.personal_recovery:nullptr,sink);}
class Prefix final:public GrowthAdjustmentFactPrefixSink {
 const WorldBundle& w_;SourcePinSink* output_;const SourcePin* requested_;
public:
 bool found=false;
 Prefix(const WorldBundle& w,SourcePinSink* out,const SourcePin* requested):w_(w),output_(out),requested_(requested){}
 Status actual_prefix_fact(std::string_view action,Integer lesson,std::string_view fact_id)override{
  const SourcePinConsumerId lookup{fact_id};const auto* head=w_.common.state().facts.find(lookup.value());
  if(action.empty()||lesson<1||!head||head->id!=fact_id||head->domain.empty()||head->kind.empty())return common_failure("GrowthOriginalFactPrefixHeadRequired",StableId(fact_id));
  const auto id=cell_key(action,lesson,fact_id);
  const SourcePinBuildView pin{"growth","study_adjustment_prefix",id.value(),"original_fact",head->domain,head->kind,head->id,SourcePinReason::unique_outcome};
  if(requested_&&same(pin,*requested_))found=true;return output_?output_->append(pin):common_success();
 }
};
Status prefixes(const WorldBundle& w,Prefix& sink){auto valid=selection(w);if(!valid.ok)return valid;return visit_growth_adjustment_fact_prefix_references(w.common,w.growth,*w.catalog->growth,sink);}
}
Status validate_growth_original_fact_prefixes(const WorldBundle& world){Prefix sink(world,nullptr,nullptr);return prefixes(world,sink);}
Outcome<std::vector<SourceConsumerRegistration>> make_growth_original_source_consumers(const std::vector<RetentionSourceKind>& header_sources){
 if(header_sources.empty())return {false,{},common_failure("CompleteOriginalHeaderSourceRegistryRequired").error};
 std::set<std::pair<StableId,StableId>> kinds;for(const auto& source:header_sources)if(source.domain.empty()||source.kind.empty()||!kinds.emplace(source.domain,source.kind).second)return {false,{},common_failure("InvalidOriginalHeaderSourceRegistry").error};
 std::vector<SourceConsumerRegistration> out;
 for(const auto slot:{GrowthCommandResultSlot::fixed_command,GrowthCommandResultSlot::study_adjustment,GrowthCommandResultSlot::study_reservation_release,GrowthCommandResultSlot::tactic_start,GrowthCommandResultSlot::tactic_closure,GrowthCommandResultSlot::recovery_closure}){
  SourceConsumerRegistration r;r.domain="growth";r.kind=slot_name(slot);r.slot="command_result";r.max_references_per_consumer=1;r.allowed_source_kinds={{"common","command_result"}};
  r.enumerate_actual_pins=[slot](const WorldBundle& w,SourcePinSink& output)->Status{Commands sink(w,slot,&output,nullptr);return commands(w,sink);};
  r.validate_actual_pin=[slot](const WorldBundle& w,const SourcePin& pin){Commands sink(w,slot,nullptr,&pin);auto valid=commands(w,sink);return !valid.ok?valid:sink.found?common_success():common_failure("GrowthCommandPinNotActual",pin.consumer.id);};out.push_back(std::move(r));
 }
 SourceConsumerRegistration prefix;prefix.domain="growth";prefix.kind="study_adjustment_prefix";prefix.slot="original_fact";prefix.max_references_per_consumer=1;prefix.allowed_source_kinds=header_sources;
 prefix.enumerate_actual_pins=[](const WorldBundle& w,SourcePinSink& output)->Status{Prefix sink(w,&output,nullptr);return prefixes(w,sink);};
 prefix.validate_actual_pin=[](const WorldBundle& w,const SourcePin& pin){Prefix sink(w,nullptr,&pin);auto valid=prefixes(w,sink);return !valid.ok?valid:sink.found?common_success():common_failure("GrowthFactPrefixPinNotActual",pin.consumer.id);};out.push_back(std::move(prefix));
 return {true,std::move(out),{}};
}
}
