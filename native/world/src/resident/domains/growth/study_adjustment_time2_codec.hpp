// 显式StudyPerformanceAdjustment/2候选入口；现有G和G+32从不调用它。
#pragma once
#include "resident/domains/growth/growth_refs.hpp"
#include "growth_result_ref_fields.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
Status visit_study_adjustment_time2(CodecVisitor&,StudyPerformanceAdjustment&,bool exact_command_cuts=false,bool capability_chain=false,GrowthContributionWireVersion=GrowthContributionWireVersion::original);
Status emit_study_adjustment_time2(ProjectionWriter&,std::string_view,const StudyPerformanceAdjustment&,bool exact_command_cuts=false,bool capability_chain=false,GrowthContributionWireVersion=GrowthContributionWireVersion::original);
Outcome<BoundaryValue> encode_study_adjustment_time2(const StudyPerformanceAdjustment&,bool exact_command_cuts=false,bool capability_chain=false,GrowthContributionWireVersion=GrowthContributionWireVersion::original);
Outcome<StudyPerformanceAdjustment> decode_study_adjustment_time2(const BoundaryValue&,bool exact_command_cuts=false,bool capability_chain=false,GrowthContributionWireVersion=GrowthContributionWireVersion::original);
}
