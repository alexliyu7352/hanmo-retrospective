// P §15.4 原有限成立输入。此处只有原条款与形状，不凭引用存在授予权利。
#pragma once
#include "resident/kernel/source_ref.hpp"
#include "resident/model/common_model.hpp"

namespace hanmo::resident {
// 成立原因闭合；非法占有与自愿投入不能借另一个字符串互换。
enum class FoundationKind {
  licensed_charter,
  voluntary_association,
  secession
};

// 起步选择保存真实原 source 引用，不复制人员、载体、钱粮或当前权限。
// 无固定据点的合法移动组织按 S6 B1.1 使用 absent，不能用空字符串替代。
struct FoundationTerms {
  FoundationKind kind = FoundationKind::voluntary_association;
  StableId organization_kind, constitution_id, initial_office_id;
  StableId initial_grant_template_id, founder;
  std::vector<SourceRef> accepted_member_refs;
  std::optional<SourceRef> base_right_ref;
  std::vector<SourceRef> mobile_carrier_refs, asset_contribution_refs;
  std::vector<SourceRef> supply_contract_refs,
      former_responsibility_dispositions;
};

// 意向仅冻结原输入；接受 ref 是后继结果，不预填未来 ID 或复制第二套权限。
// 每次更改输入必须成为另一实际意向，旧支持不能无声跟随新条款。
struct FoundationIntentTerms {
  FoundationKind kind = FoundationKind::voluntary_association;
  StableId organization_kind, constitution_id, initial_office_id;
  StableId initial_grant_template_id, founder;
  std::optional<SourceRef> base_right_ref;
  std::vector<SourceRef> mobile_carrier_refs, asset_contribution_refs;
  std::vector<SourceRef> supply_contract_refs,
      former_responsibility_dispositions;
};

// 每组织形态的原有限作者参数。required_base_kind 只指已锁目录原类别，
// 不能由本 shape 门把任意非空值解释为已实现的 base/mobile 权利来源。
struct FoundingRequirement {
  Integer min_accepted_people = 0;
  StableId required_base_kind;
  Integer runway_ticks = 0;
};

// 只保锁内每形态原要求；实际制度、初始 Office/Grant 与外域依据另有真源。
// 尚未登记原 political_continuity 目录/持久段，不可单独发布 complete-ready。
struct PoliticalFoundationRequirementCatalog {
  CatalogIdentity identity;
  OrderedTable<FoundingRequirement> requirements_by_organization_kind;
};

// 稳定 tag 供后续原 codec 使用；枚举序号绝不成为 wire 值。
const char *foundation_kind_tag(FoundationKind);
std::optional<FoundationKind> foundation_kind_from_tag(const StableId &);
// 不数引用条数冒人数，不强迫成立者以既存组织 MembershipOffer 自招。
Status validate_foundation_terms_shape(const FoundationTerms &);
Status validate_foundation_intent_terms_shape(const FoundationIntentTerms &);
// 只比原意向输入；实际接受列表必须由同版本支持原源另验，不能以此值授成立权。
bool foundation_terms_match_intent(const FoundationTerms &,
                                   const FoundationIntentTerms &);
Status validate_founding_requirement_shape(const FoundingRequirement &);
// 反读全身份相同的有限作者行。返回借值随原锁目录寿命，非能力证明。
Outcome<const FoundingRequirement *> read_original_founding_requirement(
    const PoliticalFoundationRequirementCatalog &, const CatalogIdentity &,
    const StableId &organization_kind);
} // namespace hanmo::resident
