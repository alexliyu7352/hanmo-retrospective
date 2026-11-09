// E01 原 ProductionAsset 的产权采用与实际交割。这里只保存本 owner 的
// 不可变业务来源，不保存第二份 current Owner、现金余额或设施容量。
#pragma once
#include "facility_construction_model.hpp"

namespace hanmo::resident {
struct ProductionAsset;
// 报价锁定真实产权头和双方指定钱包。报价本身不表示任何一方已同意；
// 第一版私产现金交割不承接旧债，也不隐含转移任何场所/建造许可。
struct ProductionAssetTransferTerms {
  StableId asset_ref;
  MomentOwnerSource previous_title_source;
  StableId seller_actor_id, buyer_actor_id;
  StableId payer_wallet_id, payee_wallet_id;
  Integer price = 0;
};
// 原卖方本人实际采用报价后才存在 offer；修改价格/对象须产生另一 offer，
// 不原地改已被对方引用的条款。真实命令结果在同一候选末尾绑定。
struct ProductionAssetTransferOffer {
  StableId offer_id;
  ProductionAssetTransferTerms terms;
  WorldMoment offered_at;
  SourceRef seller_command_source;
};
// 买方本人接受同一个不可变 offer；这不是代理、Task 或 Dialogue 通用同意。
struct ProductionAssetTransferAcceptance {
  StableId acceptance_id, offer_id, buyer_actor_id;
  WorldMoment accepted_at;
  SourceRef buyer_command_source;
};
// 真实现金两腿的业务结果，不是余额账户。写入前必须与同候选原
// ResourceExecutionDraft 的 WalletDelta 顺序/前后值逐项核对。
struct ProductionAssetTransferCash {
  StableId from_wallet_id, to_wallet_id;
  Integer quantity = 0;
};
// 新产权头只能由这条真实交割产生，原 owner/新 owner 从该 offer 派生。
// 初始头仍是已完整核验的原设施安装源；不补造另一份初始产权事实。
struct ProductionAssetTitleTransfer {
  StableId transfer_id, offer_id, acceptance_id;
  WorldMoment applied_at;
  ProductionAssetTransferCash actual_cash;
};
// 原 PlanFacilityCapacity 的同意来源精确限定一个资产、工人和锁内方案。
// 只由当时真实产权人本人作出；产权改变后不授权新工期，不等同永续 Grant。
struct ProductionAssetConstructionConsent {
  StableId consent_id, asset_ref, owner_actor_id, builder_actor_id;
  EconomicRuleRef construction_terms_ref;
  MomentOwnerSource title_source;
  FacilitySourceRef usage_source;
  WorldMoment consented_at;
  SourceRef owner_command_source;
};
// 首次 /1 字段草案，尚未选入 World/codec。正向/反向覆盖必须逐原 Common
// 顺序核全部来源；未知或缺失 payload 硬拒，不能让空 current 投影覆盖旧权。
struct ProductionAssetLifecycleState {
  Integer schema_version = 1;
  CheckpointVector<ProductionAssetTransferOffer> offers;
  CheckpointVector<ProductionAssetTransferAcceptance> acceptances;
  CheckpointVector<ProductionAssetTitleTransfer> transfers;
  CheckpointVector<ProductionAssetConstructionConsent> construction_consents;
};
// 同 job 借真实 owner 与原头，不复制 CurrentOwner 表。完整 reader 还须核
// 原安装、所有交割/本人命令及同 M 先后；返回引用不能进入持久根。
struct ProductionAssetTitleRead {
  const ProductionAsset *original_asset = nullptr;
  const PhysicalServiceFacilityExtension *original_facility = nullptr;
  const Owner *initial_owner = nullptr;
  const ProductionAssetTransferOffer *last_transfer_offer = nullptr;
  const ProductionAssetTitleTransfer *last_transfer = nullptr;
};
} // namespace hanmo::resident
