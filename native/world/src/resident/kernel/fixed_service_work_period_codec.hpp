// /2 的 JSON、binary、无树 emit 只走同一字段表；不改变旧 /1 出口。
#pragma once
#include "fixed_service_work_period.hpp"
#include "world_timeline_codec.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
namespace hanmo::resident {
Outcome<BoundaryValue> encode_work_period_ref2(const WorkPeriodRef2&);
Outcome<WorkPeriodRef2> decode_work_period_ref2(const BoundaryValue&);
Status visit_work_period_ref2(CodecVisitor&,WorkPeriodRef2&);
Status emit_work_period_ref2(ProjectionWriter&,std::string_view,const WorkPeriodRef2&);
Outcome<BoundaryValue> encode_work_period_view2(const WorkPeriodView2&);
Outcome<WorkPeriodView2> decode_work_period_view2(const BoundaryValue&);
Status visit_work_period_view2(CodecVisitor&,WorkPeriodView2&);
Status emit_work_period_view2(ProjectionWriter&,std::string_view,const WorkPeriodView2&);
}
