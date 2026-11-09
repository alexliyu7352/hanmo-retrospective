// 真实 S5 建设采用的 S6 历史责任切点，不授施工/资产使用许可。
#pragma once
#include "politics_takeover_history2.hpp"
namespace hanmo::resident {
struct FacilityConstructionAdoptionContext;
// 只接受原 local_economy/facility_construction_started/1；责任必须由同一
// 锁定生产计划证明实际使用该条款的资产，不借假 S3 start 或任意调用方 M。
// context 与 adoption 必须借同一候选/Common 和实际钟，外层 owner 保活。
Outcome<PoliticalResponsibilityMomentRead>
read_political_before_facility_construction(
    const PoliticalTakeoverContext &,
    const FacilityConstructionAdoptionContext &,
    const PoliticalResponsibilityRef &, const MomentOwnerSource &);
// 半开服务窗逐项保留原 Common 采用序及批准的全部 decision；无接管链只是
// 无这条 S6 历史，不代表拥有资产、已获得施工权或满足整个服务区间。
Outcome<PoliticalResponsibilityWindowRead>
read_political_facility_construction_responsibility_window(
    const PoliticalTakeoverContext &,
    const FacilityConstructionAdoptionContext &,
    const PoliticalResponsibilityRef &, const MomentOwnerSource &,
    const MomentSpan &);
} // namespace hanmo::resident
