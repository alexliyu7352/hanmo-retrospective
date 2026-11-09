// v1e §5 的原 A 投影；当前首次发送与已有 B 历史反读分别调用原 Family 门。
#pragma once
#include "family_communication_history.hpp"
namespace hanmo::resident {
// 真首次发送必经 Family 原有效性校验；不把只读 Content1 等同当前可再发送。
Outcome<FamilyCommunicationSourceView2> read_family_communication_source_v2(
    const FamilyCommunicationContentReadContext &, const SourceRef &,
    const ReadStamp &);
// 已有 B 的一次语义只核当时 A，当前已终结/换控制/换地点不重发也不倒拒旧结果。
Outcome<FamilyCommunicationSourceView2>
read_family_communication_source_before_fact_v2(
    const FamilyCommunicationHistoryReadContext &, const SourceRef &,
    const SourceRef &actual_post, const ReadStamp &);
} // namespace hanmo::resident
