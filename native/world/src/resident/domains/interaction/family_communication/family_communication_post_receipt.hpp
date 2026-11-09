// 本人 post 命令的最小真实回执；不借 A 的 FamilyEffect 或新增收知状态。
#pragma once
#include "family_communication_refs.hpp"
namespace hanmo::resident {
struct FamilyCommunicationPostReceipt1 {
  FamilyPostRef1 post_fact_ref;
  bool already_posted = false;
};
// 只核唯一回执的静态形状，不把可解码地址当成真实 B 或赋予获知权。
Status
validate_family_post_receipt1_shape(const FamilyCommunicationPostReceipt1 &);
} // namespace hanmo::resident
