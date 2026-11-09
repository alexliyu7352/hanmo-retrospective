// B1原两种本人输入的严格边界与同根分派；不是新的成立批准或独立预约命令。
#pragma once
#include "foundation_asset_candidate.hpp"
#include "foundation_asset_commands.hpp"
namespace hanmo::resident {
using FoundationAssetPersonalCommand =
    std::variant<OfferFoundationAssets, ConsentFoundationAssets>;
// kind与typed输入逐项对应，未知输入不能回退另一项成功分支。
bool foundation_asset_personal_command_kind(const StableId &) noexcept;
const char *foundation_asset_personal_command_kind(
    const FoundationAssetPersonalCommand &) noexcept;
Outcome<FoundationAssetPersonalCommand>
decode_foundation_asset_personal_command(const StableId &,
                                         const BoundaryValue &);
Outcome<BoundaryValue> encode_foundation_asset_personal_command(
    const FoundationAssetPersonalCommand &);
// 实际资源、本人表达/获知及原时刻均只读；不分配ID、不预约、不缓存成功许可。
Status preview_foundation_asset_personal_command(
    const FoundationAssetReadContext &, const StableId &actual_actor,
    const StableId &kind, const FoundationAssetPersonalCommand &,
    const ReadStamp &, const ReadStampAuthority &,
    const DomainReadVersionSource &);
// 当前候选重新预检后只写原offer/consent。外层须以原LocalEconomyEffects
// 缓存成功回执、调用原bind晚封口、完整World全验，晚失败整候选弃置。
Outcome<PendingFoundationAssetCommand>
apply_foundation_asset_personal_command_in_candidate(
    FoundationAssetWriteContext &, const StableId &actual_actor,
    const StableId &kind, const FoundationAssetPersonalCommand &,
    const ReadStamp &, const ReadStampAuthority &,
    const DomainReadVersionSource &);
} // namespace hanmo::resident
