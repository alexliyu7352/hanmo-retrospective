#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/politics/politics_projection_detail.hpp"
// 此子载荷仅随明确diplomacy/2读写；v1调用链不写新增存在位。
#include "diplomacy_v2_state.hpp"
#include "diplomacy_v2_codec_detail.hpp"

namespace hanmo::resident {
Status emit_diplomacy_v2_state_fields(ProjectionWriter& writer,const DiplomacyV2State& state){
 try{return dynamic_economy_codec::emit_fields(writer,state);}
 catch(const boundary::DecodeFailure& e){return writer.reject(e.error.original.code,e.error.original.path);}
}
BoundaryValue encode_diplomacy_v2_state(const DiplomacyV2State& state){return politics_projection_detail::encode_fields(state,emit_diplomacy_v2_state_fields);}
Outcome<DiplomacyV2State> decode_diplomacy_v2_state(const BoundaryValue& value) {
    try {
        DiplomacyV2State state; dynamic_economy_codec::decode(value, state, "/diplomacy/v2");
        if (state.version != 2) return {false, {}, common_failure("UnsupportedDiplomacyReportVersion").error};
        return {true, std::move(state), {}};
    } catch (const boundary::DecodeFailure& e) { return {false, {}, e.error}; }
}
Status visit_diplomacy_v2_checkpoint(CodecVisitor& codec, DiplomacyV2State& state) {
    auto result = dynamic_economy_codec::checkpoint(codec, "diplomacy_v2", state); if (!result.ok) return result;
    return state.version == 2 ? common_success() : codec.reject("UnsupportedDiplomacyReportVersion");
}
}
