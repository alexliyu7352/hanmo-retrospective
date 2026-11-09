// v1b§5七个原根同一paired字段表。完整世界选择来自已验证Catalog，外部请求无开关。
#pragma once
#include "study_service_model.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/ordered_value.hpp"
#include <string_view>
namespace hanmo::resident {
class ProjectionWriter;
Status visit_study_capability_state(CodecVisitor&,StudyCapabilityState&);
Status emit_study_capability_state(ProjectionWriter&,std::string_view,const StudyCapabilityState&);
Outcome<BoundaryValue> encode_study_capability_state(const StudyCapabilityState&);
Outcome<StudyCapabilityState> decode_study_capability_state(const BoundaryValue&);
// G+32原StudyFact尾的两个Optional由原Growth codec调用；旧G绝不调用本访客。
Status visit_study_service_binding_v2(CodecVisitor&,StudyServiceBindingV2&);
Status emit_study_service_binding_v2(ProjectionWriter&,std::string_view,const StudyServiceBindingV2&);
Outcome<BoundaryValue> encode_study_service_binding_v2(const StudyServiceBindingV2&);
Outcome<StudyServiceBindingV2> decode_study_service_binding_v2(const BoundaryValue&);
Status visit_study_service_terminal_v2(CodecVisitor&,StudyServiceTerminalV2&);
Status emit_study_service_terminal_v2(ProjectionWriter&,std::string_view,const StudyServiceTerminalV2&);
Outcome<BoundaryValue> encode_study_service_terminal_v2(const StudyServiceTerminalV2&);
Outcome<StudyServiceTerminalV2> decode_study_service_terminal_v2(const BoundaryValue&);
}
