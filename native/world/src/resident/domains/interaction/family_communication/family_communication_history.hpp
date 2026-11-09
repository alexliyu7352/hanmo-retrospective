// 原 §4.5 配对历史读：先核真实 B 头/行 shape，再借更早
// A，不用今日资格重写历史。
#pragma once
#include "family_communication_execution_shape.hpp"
#include "family_communication_read.hpp"
namespace hanmo::resident {
struct FamilyCommunicationHistoryReadContext {
  const FamilyCommunicationContentReadContext &content;
  const OrderedTable<FamilyCommunicationPostResult1> &original_posts;
};
// 此本地选型仅支持真实 Direct B。D 属未选承运分支，不能用伪 B 或 today 替它。
// 结果仍只是公开内容；B 的完整历史渠道/身体/route 由外层唯一 S3 validator
// 校验。
Outcome<FamilyCommunicationContent1>
read_family_communication_content_before_fact_v1(
    const FamilyCommunicationHistoryReadContext &, const SourceRef &expression,
    const FamilyCommunicationContentAudience1 &, const SourceRef &actual_post,
    const ReadStamp &);
} // namespace hanmo::resident
