// Original Common inventory title, for actual private funding/mandate readers.
// It is neither assent nor permission to spend another person's money.
#pragma once
#include "resident/storage/common_store.hpp"
#include "resident/domains/estate/estate_title_history.hpp"
#include <variant>
namespace hanmo::resident {
struct FamilyState;struct FamilyCatalog;struct MilitaryState;struct EstateCatalog;struct NavalState;struct NavalCatalog;class CurrentMomentRead;
struct AuthoredActorFoundationState;struct ActorFoundationCatalog;
struct RequiredPrivateInventoryOrigin {SourceRef original_catalog_source;};
struct AuthoredPersonalInventoryOrigin {
 ScenarioOriginRef scenario_origin;StableId actor_id,initialization_result_ref;
};
struct CreatedPersonalInventoryOrigin {
 SourceRef actual_creation_source;Integer source_schema=2;
 StableId actor_id,registration_result_ref;WorldMoment created_at;
};
using PrivateInventoryInitialOrigin=std::variant<RequiredPrivateInventoryOrigin,AuthoredPersonalInventoryOrigin,CreatedPersonalInventoryOrigin>;
struct PrivateInventoryTitleView {
 StableId inventory_id;Owner legal_owner;PrivateInventoryInitialOrigin initial_origin;
 std::vector<SourceRef> original_title_applications;
};
class PrivateInventoryTitleRead {public:virtual ~PrivateInventoryTitleRead()=default;
 virtual Outcome<PrivateInventoryTitleView> read_current_private_inventory_title(const StableId&)const=0;
 virtual Outcome<PrivateInventoryTitleView> read_private_inventory_title_before_source(const StableId&,const SourceRef& actual_adoption)const=0;
};
struct PrivateInventoryTitleContext {
 const CommonStore& common;const CommonCatalog& catalog;const FamilyState& family;
 const MilitaryState& military;const EstateCatalog& estates;const EstateAssetTitleRead& estate_titles;
 const AuthoredActorFoundationState* foundation=nullptr;
 const ActorFoundationCatalog* foundation_catalog=nullptr;
 const FamilyCatalog* family_catalog=nullptr;const CurrentMomentRead* clock=nullptr;
 const NavalState& naval;const NavalCatalog& naval_catalog;
};
// Finite original title routes: locked initial private inventory; Estate's original
// succession chain; the original authored-actor seed's generated personal inventory.
// Naval cargo inventories require the original naval title route, never this private branch.
// Created personal inventories use the actual original S4 registration. Other
// dynamic acquisition needs its original producer registration.
class NativePrivateInventoryTitleRead final:public PrivateInventoryTitleRead {
 const PrivateInventoryTitleContext& x_;
public:explicit NativePrivateInventoryTitleRead(const PrivateInventoryTitleContext&x):x_(x){}
 bool bound_to(const CommonStore& c)const noexcept{return &x_.common==&c;}
 Outcome<PrivateInventoryTitleView> read_current_private_inventory_title(const StableId&)const override;
 Outcome<PrivateInventoryTitleView> read_private_inventory_title_before_source(const StableId&,const SourceRef&)const override;
};
}
