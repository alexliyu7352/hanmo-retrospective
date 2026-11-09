// B1 私人原资源 offer/本人 consent 的真实候选 writer；不单独发布世界。
#pragma once
#include "foundation_asset_quote.hpp"
#include "foundation_asset_read.hpp"
namespace hanmo::resident {
struct CommitResult;
struct FoundationAssetWriteContext {
  CommonStore &common;
  FoundationAssetSourcesState &state;
  CheckpointMemoryCharge &owned_fields;
  const FoundationAssetReadContext &read;
};
// 只属于一次真实未发布命令的行索引，不是持久授权或可复用工作 token。
struct PendingFoundationAssetCommand {
  enum class Kind { offer, consent } kind = Kind::offer;
  std::size_t row_index = 0;
};
// 先当前真实本人表达/控制门，再锁实际私钱包/精确 lot。报价采用不预约、
// 不预承诺尚未发生的 intent，不自动使三位支持者同意贡献财产。
Outcome<PendingFoundationAssetCommand>
create_private_foundation_asset_offer_in_candidate(
    FoundationAssetWriteContext &, const StableId &actual_actor,
    const FoundationPrivateAssetRequest &, const ReadStamp &,
    const ReadStampAuthority &, const DomainReadVersionSource &);
// 本人先实际获知已发生同版 intent，才可接受被该意向冻结的既存 offer。
// 不以 FoundationSupport 或获知回执替本人财产同意，亦不预扣/预约资源。
Outcome<PendingFoundationAssetCommand>
consent_private_foundation_assets_in_candidate(
    FoundationAssetWriteContext &, const StableId &actual_actor,
    const SourceRef &original_offer, const MomentOwnerSource &actual_intent,
    Integer expected_terms_revision, const MomentOwnerSource &actual_content,
    const ReadStamp &, const ReadStampAuthority &,
    const DomainReadVersionSource &);
// 只借这次仍未封口行的 ID 给原 LocalEconomyEffects；失败整候选丢弃。
Outcome<const StableId *>
read_pending_foundation_asset_source_id(const FoundationAssetWriteContext &,
                                        const PendingFoundationAssetCommand &);
// 原 S0 成功回执缓存已存在才晚绑并立即完整反读；绑定后失败仍不可发布。
Status bind_foundation_asset_command_result_in_candidate(
    FoundationAssetWriteContext &, const PendingFoundationAssetCommand &,
    const CommitResult &);
} // namespace hanmo::resident
