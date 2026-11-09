// 原本人已接受资金与真实S1消费者由同候选拥有；只借唯一原根，不缓存时刻或第二钱包。
#pragma once
#include "world_read_stamp.hpp"
#include "resident/domains/commerce/personal_transport_funding.hpp"
#include "resident/domains/estate/estate_rest_history.hpp"
#include "resident/domains/travel/native_party_movement_cost.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
namespace hanmo::resident {
struct WorldBundle;
Outcome<Integer> selected_native_private_transport_funding_version(const CatalogIdentity&);
Status append_native_private_transport_funding_module(const BoundaryValue& scenario,CatalogIdentity&);
Status prepare_native_private_transport_funding_selection(const BoundaryValue& scenario,PreparedCatalog&);
Status validate_native_private_transport_funding_catalog(const PreparedCatalog&);
Status validate_native_private_transport_funding_layout(const WorldBundle&);
class NativeWorldPrivateTransportFundingRead final:public CheckpointAllocatedObject {
    const WorldBundle& world_;
    WorldCurrentMomentRead clock_;
    EstateRestHistoryContext estate_history_;
    NativeEstateAssetTitleRead estate_titles_;
    PrivateWalletTitleContext wallet_context_;
    NativePrivateWalletTitleRead wallet_titles_;
    SelfTransportFundingContext funding_;
    explicit NativeWorldPrivateTransportFundingRead(const WorldBundle&);
public:
    static Outcome<std::unique_ptr<NativeWorldPrivateTransportFundingRead>> create(const WorldBundle&);
    NativeWorldPrivateTransportFundingRead(const NativeWorldPrivateTransportFundingRead&)=delete;
    NativeWorldPrivateTransportFundingRead& operator=(const NativeWorldPrivateTransportFundingRead&)=delete;
    const SelfTransportFundingContext& funding()const noexcept{return funding_;}
    Status validate()const;
};
class NativeWorldPersonalTransportFundingRead final:public CheckpointAllocatedObject {
    const WorldBundle& world_;
    std::unique_ptr<NativeWorldPrivateTransportFundingRead> funding_;
    NativeMovementActionTimeRead movement_;
    NativePartyPersonalMovementCostHistoryRead party_;
    TaskPersonalMovementFundingContext context_;
    NativeTaskPersonalMovementFundingRead original_;
    NativeWorldPersonalTransportFundingRead(const WorldBundle&,std::unique_ptr<NativeWorldPrivateTransportFundingRead>);
public:
    static Outcome<std::unique_ptr<NativeWorldPersonalTransportFundingRead>> create(const WorldBundle&);
    NativeWorldPersonalTransportFundingRead(const NativeWorldPersonalTransportFundingRead&)=delete;
    NativeWorldPersonalTransportFundingRead& operator=(const NativeWorldPersonalTransportFundingRead&)=delete;
    const PersonalMovementFundingRead& funding()const noexcept{return original_;}
    const PersonalMovementPartyCostHistoryRead& party_history()const noexcept{return party_;}
    const TaskPersonalMovementFundingContext& original_context()const noexcept{return context_;}
    Status validate()const;
};
Status validate_native_private_transport_funding_world(const WorldBundle&);
}
