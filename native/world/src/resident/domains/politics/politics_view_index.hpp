// 仅保存同一已提交世界的合法读取ID；不入银行，不复制任命状态、财政数值或人物位置。
#pragma once
#include "hanmo/resident/checkpoint_memory.hpp"
#include "politics_model.hpp"
#include <map>
namespace hanmo::resident {
struct SocialState;
struct PoliticalFiscalState;
struct PoliticsViewIndex {
  WorldIdentity identity;
  std::size_t fact_count = 0, appointment_count = 0, policy_count = 0;
  CheckpointMap<StableId, std::vector<StableId>>
      known_appointment_facts_by_actor, jurisdiction_ids_by_site,
      jurisdiction_ids_by_issuer, office_ids_by_actor, actor_ids_by_site;
  CheckpointMap<StableId, StableId> latest_policy_by_jurisdiction;
};
PoliticsViewIndex
make_politics_view_index(const CommonStore &, const PoliticsState &,
                         const OrganizationState &, const OrganizationCatalog &,
                         const SocialState &,
                         const PoliticalFiscalState *fiscal = nullptr);
// 仅热读投影借本次实际钟，不持久化，不把候选枚举当授权证明。
Outcome<PoliticsViewIndex> make_politics_current_view_index(
    const CommonStore &, const PoliticsState &, const OrganizationState &,
    const OrganizationCatalog &, const SocialState &,
    const OrganizationAuthorityClock &,
    const PoliticalFiscalState *fiscal = nullptr);
bool politics_view_index_matches(const PoliticsViewIndex &, const CommonStore &,
                                 const PoliticsState &);
// 只适配既有 map 的 allocator 差异，仍沿同一只读查找与缺省值。
template <class Allocator>
inline const std::vector<StableId> &
politics_index_rows(const std::map<StableId, std::vector<StableId>,
                                   std::less<StableId>, Allocator> &table,
                    const StableId &key) {
  static const std::vector<StableId> empty;
  const auto found = table.find(key);
  return found == table.end() ? empty : found->second;
}
} // namespace hanmo::resident
