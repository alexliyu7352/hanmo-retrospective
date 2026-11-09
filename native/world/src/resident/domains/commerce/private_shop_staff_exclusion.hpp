// 庄园任职/雇用只借私铺原合同前界；纯本账读不反调Shop、Estate、Family或工资全验。
#pragma once
#include "private_shop_staff_contract.hpp"
namespace hanmo::resident {
class CommonStore;struct DynamicStepInput;struct PrivateShopState;
struct PrivateShopWorkerCommitment {StableId contract_id,shop_id,worker_actor_id,acceptance_fact_id,terminal_fact_id;Integer accepted_tick=0,terminal_tick=-1;};
struct PrivateShopWorkerCommitments {WorldIdentity source_identity;StableId worker_actor_id;Integer observed_tick=0;std::vector<PrivateShopWorkerCommitment> contracts;};
class PrivateShopWorkerCommitmentRead {
public:virtual ~PrivateShopWorkerCommitmentRead()=default;
 virtual Outcome<PrivateShopWorkerCommitments> private_shop_worker_commitments(const StableId& worker)const=0;
};
class PrivateShopWorkerCommitmentAccess final:public PrivateShopWorkerCommitmentRead {
public:PrivateShopWorkerCommitmentAccess(const CommonStore& common,const PrivateShopState& state,const DynamicStepInput* phase=nullptr):common_(common),state_(state),phase_(phase){}
 Outcome<PrivateShopWorkerCommitments> private_shop_worker_commitments(const StableId&)const override;
private:const CommonStore& common_;const PrivateShopState& state_;const DynamicStepInput* phase_;
};
// 冷口只认原Estate管理/生产与Employment接受的具名头；调用者先核该载荷actor/tick。
Status guard_private_shop_worker_commitment(const CommonStore&,const PrivateShopWorkerCommitmentRead*,const StableId& worker,const StableId& before_fact={},Integer tick=-1,const DynamicStepInput* phase=nullptr);
}
