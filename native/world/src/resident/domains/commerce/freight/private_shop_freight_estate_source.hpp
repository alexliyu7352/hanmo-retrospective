// 原私人庄园存取货的同候选来源；搬入庄园不等于商铺到货，更不授运费归偿。
#pragma once
#include "resident/domains/estate/estate_model.hpp"
#include "resident/storage/resource_execution_draft.hpp"
#include <memory>
namespace hanmo::resident {
struct ResourceExecutionCaptureIdentity;
// 领域原准入与完整Estate史由调用者先验；这里不回调Shop或修改任何资源。
struct PrivateShopFreightEstateContext {
 const CommonStore& common;const EstateState& state;const EstateCatalog& catalog;
};
// 原preview成功后、原apply_estate之前取值；只在这一候选栈内有效，不可持久化为历史证据。
struct PrivateShopFreightEstateBoundary {
 const CommonStore* original_common=nullptr;const EstateState* original_state=nullptr;const EstateCatalog* original_catalog=nullptr;
 WorldIdentity identity;StableId actor_id,estate_id,estate_place_id;
 EstateTransfer command;EstateTransferTerms terms;
 InventoryView source_before,destination_before;
 Integer next_instance_before=0;std::size_t recipe_begin=0;
    // 原活动capture唯一身份；结束重开、copy/assignment与move均不能复用这个前界。
    std::shared_ptr<const ResourceExecutionCaptureIdentity> capture_identity;
};
// 原source/destination各批映射在actual_recipe内；跨recipe连续、原委托与到店仍是另外的必需来源。
struct PrivateShopFreightEstateSource {
 WorldIdentity source_identity;StableId original_fact_id;
 EstateManagementFact original_transfer;ResourceExecutionDraft actual_recipe;
};
// 首片只接本人拥有庄园、本人行囊与同Owner私人货；原经理代办/公物转移仍沿原业务，不能套私人策略。
Outcome<PrivateShopFreightEstateBoundary> capture_private_shop_freight_estate_boundary(
 const PrivateShopFreightEstateContext&,const StableId& actor_id,const EstateQuote& actual_quote);
// 原实际EstateTransfer写成后即时消费：唯一TransferLots、原相邻estate头及完整两容器逐值匹配。
// 无头、失败recipe、额外recipe、原Owner已变或旧同品货冒新批均拒；不读今日地点补历史。
Outcome<PrivateShopFreightEstateSource> read_private_shop_freight_estate_transfer(
 const PrivateShopFreightEstateContext&,const PrivateShopFreightEstateBoundary&,const StableId& actual_fact_id);
}
