// 原 OfferResidence 的实际来源与候选 writer；报价不占床，不替对方接受。
#pragma once
#include "residence_admission_read.hpp"
#include "residence_personal_source.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/domains/travel/actor_spatial_source_membership.hpp"
#include <span>
namespace hanmo::resident {
struct ResidenceOfferContext {
  const ResidenceServiceState &state;
  const ResidenceAdmissionReadContext &admission;
  const ResidencePersonalContext &personal;
  const ActorSpatialHeadSourceReadContext &spatial;
  const BattleActorSpatialRead &battle;
  const MilitarySpatialHandoffState *military_handoffs = nullptr;
};
// 只借本次命令参数，不持久化未来接受、床位或人口选择 ID。
struct OfferResidenceRequest {
  const ActorFoundationRuleRef &terms_ref;
  std::span<const ResidenceResidentRequest> residents;
};
struct ResidenceOfferWriteContext {
  CommonStore &common;
  ResidenceServiceState &state;
  CheckpointMemoryCharge &owned_fields;
  const ResidenceOfferContext &read;
};
struct PendingResidenceOffer {
  std::size_t row_index = 0;
};
// 同一原前置核用于 preview 与真实 writer；不签可跨候选使用的许可 token。
Status preview_offer_residence(const ResidenceOfferContext &,
                               const StableId &host,
                               const OfferResidenceRequest &, const ReadStamp &,
                               const ReadStampAuthority &,
                               const DomainReadVersionSource &);
Outcome<PendingResidenceOffer>
offer_residence_in_candidate(ResidenceOfferWriteContext &, const StableId &host,
                             const OfferResidenceRequest &, const ReadStamp &,
                             const ReadStampAuthority &,
                             const DomainReadVersionSource &);
Outcome<const StableId *>
read_pending_residence_offer_id(const ResidenceOfferWriteContext &,
                                const PendingResidenceOffer &);
struct CommitResult;
// S0 原成功缓存晚绑；其后任何失败必须丢弃整个未发布 World 候选。
Status
bind_residence_offer_command_result_in_candidate(ResidenceOfferWriteContext &,
                                                 const PendingResidenceOffer &,
                                                 const CommitResult &);
// 完整实际 offer：同版许可、本人控制/表达、真实原场所及 S0 命令封口。
Outcome<const ResidenceOffer *>
read_residence_offer_source(const ResidenceOfferContext &,
                            const MomentOwnerSource &);
} // namespace hanmo::resident
