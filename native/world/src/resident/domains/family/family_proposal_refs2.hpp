// 原家庭提议地址与有限答复值；只提原持值定义，不授予来源或同意。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
// 原同锁模板 ID 与明确版本，不拿旧 authored dialogue/node 冒新模板。
struct FamilyTermsTemplateRef {
  StableId template_id;
  Integer version = 1;
};

// FamilyProposalRef/2 的六个原分支；字段地址正确并不证明 source 已实际存在。
struct FamilyMarriageProposalRef2 {
  StableId id;
};
struct FamilyBirthProposalRef2 {
  StableId id;
};
struct FamilyAdultAdoptionProposalRef2 {
  StableId id;
};
struct FamilyCollateralProposalRef2 {
  StableId id;
};
struct FamilyApprenticeshipProposalRef2 {
  StableId id;
};
struct FamilyEducationProposalRef2 {
  StableId id;
  Integer terms_version = 1;
};
using FamilyProposalRef2 =
    std::variant<FamilyMarriageProposalRef2, FamilyBirthProposalRef2,
                 FamilyAdultAdoptionProposalRef2, FamilyCollateralProposalRef2,
                 FamilyApprenticeshipProposalRef2, FamilyEducationProposalRef2>;

// wire 编号来自原完整 PersonalReply2；本旁支表只允许 refused/withdrawn。
enum class FamilyPersonalReplyDecision2 {
  accepted,
  refused,
  withdrawn,
  negotiated
};
} // namespace hanmo::resident
