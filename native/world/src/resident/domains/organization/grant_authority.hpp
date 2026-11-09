// 新授权只沿原制度根缩限；授额度不改钱货、不生成预算，也不洗掉原已支用。
#pragma once
#include "office_authority.hpp"
namespace hanmo::resident {
struct GrantChangeCommand {
  GrantChangeKind kind = GrantChangeKind::issue;
  ActingAuthority acting;
  StableId policy_id, grant_id;
  Grant terms;
  std::optional<StableId> parent_grant_id;
  Integer expected_grant_revision = 0, expected_parent_revision = 0,
          expected_office_epoch = 0;
};
// 纯预检复用唯一writer的原授权验证；不分配实例或修改原Grant/事实/版本索引。
Status preview_grant_change(const CommonStore &, const OrganizationState &,
                            const OrganizationCatalog &,
                            const GrantChangeCommand &,
                            const WorldMoment &actual_at,
                            const OrganizationAuthorityClock &);
// 真源上下文在调用者原候选中；窗口由真实当前Moment半日投影校验而非请求自报发生时刻。
Outcome<StableId> apply_grant_change_in_candidate(
    CommonStore &, OrganizationState &, const OrganizationCatalog &,
    const GrantChangeCommand &, const WorldMoment &actual_at,
    const OrganizationAuthorityClock &);
Status initialize_grant_authority(const CommonStore &, OrganizationState &,
                                  const OrganizationCatalog &);
Outcome<GrantRuntimeRecord> read_current_grant(const CommonStore &,
                                               const OrganizationState &,
                                               const OrganizationCatalog &,
                                               const StableId &grant_id);
// 同一当前父链原校验，返回真实根条款指针；仅同步只读消费，不跨任何组织写入。
Outcome<const Grant *> read_current_grant_terms_borrowed(
    const CommonStore &, const OrganizationState &, const OrganizationCatalog &,
    const StableId &grant_id);
Status
validate_grant_authority_history(const CommonStore &, const OrganizationState &,
                                 const OrganizationCatalog &,
                                 const OrganizationAuthorityClock * = nullptr);
// 原Grant条款精确子集关系；一种command/资源不够不得拼接另一根。
bool grant_terms_within(const Grant &child, const Grant &parent);
} // namespace hanmo::resident
