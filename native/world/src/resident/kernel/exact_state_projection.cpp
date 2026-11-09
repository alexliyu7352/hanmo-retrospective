#include "exact_state_projection.hpp"
#include "world_timeline_codec_fields.hpp"
#include "resident/facility_claims/facility_claims_codec_fields.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
namespace hanmo::resident {
namespace {
template<class T,class Visit>Status emit(ProjectionWriter& writer,std::string_view key,const T& value,Visit visit){
    try{return emit_projection_record<ExactStructureProjectionArchive>(writer,key,value,visit);}
    catch(exact_fields::CodecFailure& failure){return {false,{},std::move(failure.error)};}
}
}
Status emit_world_timeline2_projection(ProjectionWriter& writer,std::string_view key,const WorldTimelineState& value){return emit(writer,key,value,[](auto& archive,auto& row){return time_fields::timeline(archive,row);});}
Status emit_action_elapsed2_projection(ProjectionWriter& writer,std::string_view key,const ActionElapsedStore& value){return emit(writer,key,value,[](auto& archive,auto& row){return time_fields::elapsed(archive,row);});}
Status emit_facility_claims2_projection(ProjectionWriter& writer,std::string_view key,const FacilityClaimsState& value){return emit(writer,key,value,[](auto& archive,auto& row){return facility_fields::state(archive,row);});}
}
