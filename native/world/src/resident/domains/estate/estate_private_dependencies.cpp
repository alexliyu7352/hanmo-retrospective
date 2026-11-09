// 原庄园没有第二本雇工/订单余额；未结建设沿原行动头、冻结条款和开工事实逐一反核。
#include "estate_private_dependencies.hpp"
#include <set>
namespace hanmo::resident {
Outcome<EconomicPrivateAccountDependencies> read_estate_private_account_dependencies(const CommonStore& common,const EstateState& state,const EstateCatalog& catalog,const CommonCatalog& common_catalog,const EconomicPrivateAccountSelection& selection,const EstateAccess* access){
 using Result=Outcome<EconomicPrivateAccountDependencies>;auto fail=[&](const StableId& code,const StableId& id=StableId{})->Result{return {false,{},common_failure(code,id).error};};
 if(selection.actor_id.empty()||!common.actor(selection.actor_id)||(selection.wallet_ids.empty()&&selection.inventory_ids.empty()))return fail("InvalidEstatePrivateSelection");
 const Owner owner{Owner::Kind::actor,selection.actor_id,{}};std::set<StableId> wallets,inventories;
 for(const auto& id:selection.wallet_ids){const auto* value=common.wallet(id);if(!value||!wallets.insert(id).second||!owner_equal(value->owner,owner))return fail("EstatePrivateWalletOwnerMismatch",id);}
 for(const auto& id:selection.inventory_ids){const auto* value=common.inventory(id);if(!value||!inventories.insert(id).second||!owner_equal(value->owner,owner))return fail("EstatePrivateInventoryOwnerMismatch",id);}
 auto valid=validate_estate(common,state,catalog,common_catalog,access);if(!valid.ok)return {false,{},valid.error};
 // 不能以删去本域行动载荷把真实Common建设行动伪装成无责任。
 for(const auto& row:common.state().actions.rows())if(row.second.kind=="estate_build"&&!state.actions.find(row.first))return fail("MissingEstatePrivateBuildPayload",row.first);
 EconomicPrivateAccountDependencies out;out.source_identity=common.state().identity;out.actor_id=selection.actor_id;
 for(const auto& row:catalog.estates.rows())if(wallets.count(row.second.wallet_id)||inventories.count(row.second.inventory_id))out.blockers.push_back({"estate_bound_account",row.first,0});
 for(const auto& row:state.actions.rows()){const auto& terms=row.second.terms;if(wallets.count(terms.payer_wallet_id)||wallets.count(terms.payee_wallet_id)||inventories.count(terms.inventory_id))out.blockers.push_back({"estate_unfinished_construction",row.first,0});}
 return {true,std::move(out),{}};
}
}
