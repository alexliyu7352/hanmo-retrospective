// 新Life W原期间source body成对访问；不注册新world根、不覆盖C3 WorkPeriodRef/1。
#pragma once
#include "life_work_period.hpp"
#include "resident/checkpoint/ordered_value.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
namespace hanmo::resident {
Outcome<BoundaryValue> encode_life_work_period_source(const LifeWorkPeriodView&);
Outcome<LifeWorkPeriodView> decode_life_work_period_source(const BoundaryValue&);
Status visit_life_work_period_source(CodecVisitor&,LifeWorkPeriodView&);
Outcome<BoundaryValue> encode_life_work_period_source_ref(const LifeWorkPeriodSourceRef&);
Outcome<LifeWorkPeriodSourceRef> decode_life_work_period_source_ref(const BoundaryValue&);
Status visit_life_work_period_source_ref(CodecVisitor&,LifeWorkPeriodSourceRef&);
}
