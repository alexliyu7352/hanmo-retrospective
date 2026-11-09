#include "resident/domains/growth/growth_model.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "growth_capability_application_codec.hpp"
#include "study_adjustment_time2_codec.hpp"
#include "study_lesson_capability_codec_detail.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
namespace hanmo::resident {namespace {
template<class C>Status record(C& c,StudyPerformanceAdjustment& v,bool exact_command_cuts,bool capability_chain,GrowthContributionWireVersion wire){
 c.field("settled_tick",v.settled_tick);c.field("xp_before",v.xp_before);c.field("performance_bp",v.performance_bp);c.field("xp_gained",v.xp_gained);c.sequence("field_order",v.field_order,[](auto& x,auto& s){return x.field("value",s);});
 c.optional("exact_binding",v.exact_binding,[&](auto& x,auto& b){x.object("settled_at",[&](auto& y){return study_lesson_codec::moment(y,b.settled_at);});study_lesson_codec::text(x,"result_id",b.result_id);study_lesson_codec::text(x,"actor_id",b.actor_id);study_lesson_codec::text(x,"command_kind",b.command_kind);x.field("request_seq",b.request_seq);x.field("revision",b.revision);x.field("application_ordinal",b.application_ordinal);
 if(exact_command_cuts){if(x.reading())b.original_fact_prefix.emplace();if(!b.original_fact_prefix)return x.reject("StudyAdjustmentFactPrefixRequired");auto& cut=*b.original_fact_prefix;x.field("original_fact_prefix_size",cut.original_fact_prefix_size);x.optional("last_fact_id",cut.last_fact_id,[](auto& y,auto& id){return study_lesson_codec::text(y,"value",id);});if(x.status().ok&&(cut.original_fact_prefix_size<0||(cut.original_fact_prefix_size==0)!=!cut.last_fact_id))return x.reject("StudyAdjustmentFactPrefixShapeInvalid");}
 else if(b.original_fact_prefix)return x.reject("StudyAdjustmentFactPrefixPhysicalSelectionRequired");if(capability_chain){if(!exact_command_cuts)return x.reject("GrowthCapabilityCommandCutsRequired");x.optional("capability_application",b.capability_application,[&](auto& y,auto& a){return growth_capability_codec::application(y,a,wire);});if(x.status().ok&&bool(b.capability_application)!=(v.xp_gained>0))return x.reject("StudyAdjustmentCapabilityApplicationMismatch");}else if(b.capability_application)return x.reject("GrowthCapabilityPhysicalSelectionRequired");if(x.status().ok&&(b.request_seq<1||b.revision<1||b.application_ordinal<1||b.application_ordinal>6))return x.reject("StudyAdjustmentExactBindingShapeInvalid");return x.status();});
 if(c.status().ok&&(v.settled_tick<0||v.xp_before<0||v.xp_before>1200||v.xp_gained<0||v.xp_gained>1200-v.xp_before||(v.exact_binding&&v.exact_binding->settled_at.half_day_tick!=v.settled_tick)))return c.reject("StudyAdjustmentTime2ShapeInvalid");return c.status();
}
}
// Binary冷读由外层Growth私有candidate持有；不在本record再克隆或声称部分目标可发布。
Status visit_study_adjustment_time2(CodecVisitor& c,StudyPerformanceAdjustment& v,bool exact_command_cuts,bool capability_chain,GrowthContributionWireVersion wire){return record(c,v,exact_command_cuts,capability_chain,wire);}
Status emit_study_adjustment_time2(ProjectionWriter& sink,std::string_view name,const StudyPerformanceAdjustment& v,bool exact_command_cuts,bool capability_chain,GrowthContributionWireVersion wire){return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,v,[&](auto& c,auto& value){return record(c,value,exact_command_cuts,capability_chain,wire);});}
Outcome<BoundaryValue> encode_study_adjustment_time2(const StudyPerformanceAdjustment& value,bool exact_command_cuts,bool capability_chain,GrowthContributionWireVersion wire){BoundaryValue output;BoundaryProjectionWriter sink(output);auto status=emit_study_adjustment_time2(sink,"",value,exact_command_cuts,capability_chain,wire);if(status.ok)status=sink.finish();return {status.ok,std::move(output),status.error};}
Outcome<StudyPerformanceAdjustment> decode_study_adjustment_time2(const BoundaryValue& value,bool exact_command_cuts,bool capability_chain,GrowthContributionWireVersion wire){StudyPerformanceAdjustment output;ActorFoundationJsonVisitor c(value);auto status=record(c,output,exact_command_cuts,capability_chain,wire);if(status.ok)status=c.finish();return {status.ok,std::move(output),status.error};}
}
