// 原 Family A 的公开内容投影；本 reader 不建立 Report/消息/收知，也不替原
// Family 核许可。
#pragma once
#include "family_communication_model.hpp"
#include "resident/domains/family/collateral_personal_source.hpp"
#include "resident/kernel/read_stamp.hpp"
namespace hanmo::resident {
struct FamilyCommunicationContentReadContext {
  const CommonStore &common;
  const ReadStampAuthority &authority;
  const DomainReadVersionSource &versions;
  const CollateralPersonalSourceRead *collateral = nullptr;
};
// 当前只接原旁支三源；缺原 Family provider 或未施工 carrier
// 明确拒绝，不伪造公开内容。
Outcome<FamilyCommunicationContent1> read_family_communication_content_v1(
    const FamilyCommunicationContentReadContext &, const SourceRef &,
    const FamilyCommunicationContentAudience1 &, const ReadStamp &);
} // namespace hanmo::resident
