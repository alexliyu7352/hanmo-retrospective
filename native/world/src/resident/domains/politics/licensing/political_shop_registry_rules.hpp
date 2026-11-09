// 接册与新处分分离：本命令只接受完整旧册，保原通缉、解除和许可的所有来源。
#pragma once
#include "political_shop_registry_model.hpp"
#include "political_shop_registry_control.hpp"
#include "political_shop_registry_sources.hpp"
namespace hanmo::resident {
struct PoliticalShopRegistryContext {
 const PoliticalShopLicensingContext& legal;
 const PoliticalShopControlContext& control;
 const PoliticalShopRegistrySuccessionCatalog& catalog;
 const PoliticalShopRegistrySuccessionState& state;
};
Status validate_political_shop_registry_catalog(const PoliticalShopRegistryContext&);
Status validate_political_shop_registry_successions(const PoliticalShopRegistryContext&);
Status validate_political_shop_registry_fact_header(const CommonStore&,const PoliticalShopRegistrySuccessionState&,const FactRecord&);
Status validate_political_shop_registry_receipts(const PoliticalShopRegistryContext&,const std::function<bool(const CommitResult&)>& is_done);
Status validate_political_shop_registry_player_bindings(const CommonStore&,const PoliticalShopRegistrySuccessionState&);
Outcome<StableId> political_shop_registry_new_player_acceptance(const PoliticalShopRegistrySuccessionState& before,
 const PoliticalShopRegistrySuccessionState& after,const StableId& actor,Integer tick);
Status bind_political_shop_registry_player_receipt(CommonStore&,PoliticalShopRegistrySuccessionState&,
 const StableId& acceptance_id,const StableId& actor,Integer request_sequence,const CommitResult&,
 const std::function<bool(const CommitResult&)>& is_done);
Outcome<PoliticalShopRegistryAcceptanceQuote> preview_political_shop_registry_acceptance(
 const PoliticalShopRegistryContext&,const PoliticalTakeoverRead&,const StableId& actor,const PoliticalAcceptShopRegistry&);
Outcome<StableId> apply_political_shop_registry_acceptance_in_candidate(
 CommonStore&,PoliticalShopRegistrySuccessionState&,const PoliticalShopRegistryContext&,const PoliticalTakeoverRead&,
 const StableId& actor,const PoliticalAcceptShopRegistry&,const PoliticalShopRegistryAcceptanceQuote&);
class PoliticalShopRegistryAccess final:public PoliticalShopRegistrySuccessionRead {
public:
 explicit PoliticalShopRegistryAccess(PoliticalShopRegistryContext x):context_(x){}
 Outcome<std::optional<PoliticalShopRegistryExecutionSource>> shop_registry_current(const StableId&)const override;
 Outcome<std::optional<PoliticalShopRegistryExecutionSource>> shop_registry_before_legal_fact(const StableId&,const StableId&,Integer)const override;
private:PoliticalShopRegistryContext context_;
};
}
