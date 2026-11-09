// Original Common wallet title, for actual private funding/mandate readers.
// It is neither assent nor permission to spend another person's money.
#pragma once
#include "resident/storage/common_store.hpp"
#include "resident/domains/estate/estate_title_history.hpp"
#include <variant>
namespace hanmo::resident {
struct FamilyState;struct FamilyCatalog;struct MilitaryState;struct EstateCatalog;class CurrentMomentRead;
struct AuthoredActorFoundationState;struct ActorFoundationCatalog;
struct RequiredPrivateWalletOrigin {SourceRef original_catalog_source;};
struct AuthoredPersonalWalletOrigin {
 ScenarioOriginRef scenario_origin;StableId actor_id,initialization_result_ref;
};
struct CreatedPersonalWalletOrigin {
 SourceRef actual_creation_source;Integer source_schema=2;
 StableId actor_id,registration_result_ref;WorldMoment created_at;
};
using PrivateWalletInitialOrigin=std::variant<RequiredPrivateWalletOrigin,AuthoredPersonalWalletOrigin,CreatedPersonalWalletOrigin>;
struct PrivateWalletTitleView {
 StableId wallet_id;Owner legal_owner;PrivateWalletInitialOrigin initial_origin;
 std::vector<SourceRef> original_title_applications;
};
class PrivateWalletTitleRead {public:virtual ~PrivateWalletTitleRead()=default;
 virtual Outcome<PrivateWalletTitleView> read_current_private_wallet_title(const StableId&)const=0;
 virtual Outcome<PrivateWalletTitleView> read_private_wallet_title_before_source(const StableId&,const SourceRef& actual_adoption)const=0;
};
struct PrivateWalletTitleContext {
 const CommonStore& common;const CommonCatalog& catalog;const FamilyState& family;
 const MilitaryState& military;const EstateCatalog& estates;const EstateAssetTitleRead& estate_titles;
 const AuthoredActorFoundationState* foundation=nullptr;
 const ActorFoundationCatalog* foundation_catalog=nullptr;
 const FamilyCatalog* family_catalog=nullptr;const CurrentMomentRead* clock=nullptr;
};
// Finite original title routes: locked initial private wallet; Estate's original
// succession chain; the original authored-actor seed's generated personal wallet.
// Created personal wallets use the actual original S4 registration. Other
// dynamic acquisition needs its original producer registration.
class NativePrivateWalletTitleRead final:public PrivateWalletTitleRead {
 const PrivateWalletTitleContext& x_;
public:explicit NativePrivateWalletTitleRead(const PrivateWalletTitleContext&x):x_(x){}
 Outcome<PrivateWalletTitleView> read_current_private_wallet_title(const StableId&)const override;
 Outcome<PrivateWalletTitleView> read_private_wallet_title_before_source(const StableId&,const SourceRef&)const override;
};
}
