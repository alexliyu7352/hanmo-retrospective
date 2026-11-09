// 本域严格事实、历史签字与辖区界限；没有司法来源时不把缺省解释为无通缉。
#pragma once
#include "political_shop_licensing.hpp"
#include "political_shop_registry_contract.hpp"
#include "political_shop_registry_endpoint_owned.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include <set>
namespace hanmo::resident::shop_law_detail {
inline bool has(const std::vector<StableId> &values, const StableId &id) {
  return std::find(values.begin(), values.end(), id) != values.end();
}
inline bool same_request(const PrivateShopPermissionRequest &a,
                         const PrivateShopPermissionRequest &b) {
  return a.actor_id == b.actor_id && a.premises_id == b.premises_id &&
         a.place_id == b.place_id;
}
inline bool same_identity(const WorldIdentity &a, const WorldIdentity &b) {
  return a.world_id == b.world_id && a.execution_epoch == b.execution_epoch &&
         a.revision == b.revision && a.tick == b.tick;
}
// 只有本域六种精确头和S5实际开设头可以作历史截面。
StableId prefix_for(const StableId &kind);
Outcome<Integer> fact_sequence(const CommonStore &, const StableId &,
                               const StableId &kind);
bool before(const CommonStore &, const StableId &, const StableId &);
Status person_alive_before(const PoliticalShopLicensingContext &,
                           const StableId &actor, Integer tick,
                           const StableId &boundary);
Status stable_court_before(const PoliticalShopLicensingContext &,
                           const PoliticalShopCourt &, Integer tick,
                           Integer sequence, const StableId &boundary);
// 与旧查询同一来源算法；正式读持有8字段租约直到消费/独立复制完毕。
Outcome<OwnedPoliticalShopRegistryEndpoint>
court_endpoint_owned(const PoliticalShopLicensingContext &,
                     const PoliticalShopCourt &, Integer tick, Integer sequence,
                     const StableId &boundary);
Outcome<PoliticalShopRegistryEndpoint>
court_endpoint(const PoliticalShopLicensingContext &,
               const PoliticalShopCourt &, Integer tick, Integer sequence,
               const StableId &boundary);
Status historical_authority(const PoliticalShopLicensingContext &,
                            const PoliticalShopCourt &, const ActingAuthority &,
                            const StableId &grant, const StableId &command,
                            Integer tick, Integer sequence,
                            const StableId &boundary);
// 返回原Office.grant_ids内真实ID；仅同步借用，不跨组织写入。
Outcome<const StableId *>
current_authority_id(const PoliticalShopLicensingContext &,
                     const PoliticalShopCourt &, const ActingAuthority &,
                     const StableId &command);
Outcome<StableId> current_authority(const PoliticalShopLicensingContext &,
                                    const PoliticalShopCourt &,
                                    const ActingAuthority &,
                                    const StableId &command);
const PoliticalShopRegistry *registry(const PoliticalShopLicensingState &,
                                      const StableId &court);
bool wanted_before(const PoliticalShopLicensingContext &, const StableId &court,
                   const StableId &actor, Integer sequence);
// 正式内部许可读保活原五文本拥有返回，旧裸返回仅兼容。
Outcome<OwnedPrivateShopPermissionSource>
permission_owned(const PoliticalShopLicensingContext &,
                 const PrivateShopPermissionRequest &, Integer tick,
                 Integer sequence, const StableId &boundary);
Outcome<PrivateShopPermissionSource>
permission(const PoliticalShopLicensingContext &,
           const PrivateShopPermissionRequest &, Integer tick, Integer sequence,
           const StableId &boundary);
} // namespace hanmo::resident::shop_law_detail
