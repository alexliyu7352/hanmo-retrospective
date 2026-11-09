// 救济资源责任独立持久化；唯一任务状态和劳动钟仍从S3读取。
#pragma once
#include "public_relief.hpp"
#include "resident/domains/local_economy/local_economy_model.hpp"
#include "resident/domains/tasks/governance_task_contract.hpp"
#include "resident/domains/politics/politics_governance_contract.hpp"
namespace hanmo::resident {
struct EconomicTakeoverReadContext;struct EconomicTakeoverExecutionState;struct EconomicTakeoverPhaseContext;
// 明确分配对象与份额，不按当前人口或临时不足擅自改写已接受的完整支出。
struct PublicReliefPolicy {
    StableId id,site_id,organization_id,item_id;
    std::vector<HouseholdReliefRecipient> recipients;
};
struct PublicReliefCatalog {Integer version=1;OrderedTable<PublicReliefPolicy> policies;};
// 目录关联与原受领共用实际家庭/cohort/私产三方来源，不按名称或组织Grant猜产权。
Status validate_public_relief_policy_recipients(const CommonStore&,const LocalEconomyState&,const PublicReliefPolicy&);
// 接受时冻结真实家庭容器；事后产权变化不能将同名责任导向另一个钱包。
struct PublicReliefCommitment {
    PublicReliefTerms terms;
    StableId policy_id;
    Integer reserved_tick=0;
    std::vector<HouseholdReliefTransfer> transfers;
};
struct PublicReliefRelease {StableId id,terms_id;Integer tick=0;};
struct PublicReliefState {
    Integer version=1;
    OrderedTable<PublicReliefCommitment> commitments;
    OrderedTable<PublicReliefReceipt> receipts;
    OrderedTable<PublicReliefRelease> releases;
};
// 受领按钮先核真实当前批准、家庭计划和可用公产；无需Task或接受事实，不生成ID或预留。
Status preview_public_relief_resources(const CommonStore&,const LocalEconomyState&,const PublicReliefCatalog&,const PublicReliefState&,const GovernanceAuthorityRead&,const StableId& authority_fact_id,const StableId& executor_actor_id,Integer expected_world_revision);
// 纯报价先核真实接受源及足额资源；报价不分配ID，不更改预留。
Outcome<PublicReliefCommitment> preview_public_relief(const CommonStore&,const LocalEconomyState&,const PublicReliefCatalog&,const PublicReliefState&,const TaskGovernanceRead&,const GovernanceAuthorityRead&,const StableId& task_id);
// 由S3受领候选回调；若后续Action开工失败，中央必须丢弃整个跨域候选。
Outcome<PublicReliefTerms> reserve_public_relief(CommonStore&,const LocalEconomyState&,const PublicReliefCatalog&,PublicReliefState&,const TaskGovernanceRead&,const GovernanceAuthorityRead&,const StableId& task_id);
// 已有外层WorldBundle候选/Common保存点专用；失败须一并撤回本域状态和共同写集。
Outcome<PublicReliefTerms> reserve_public_relief_in_candidate(CommonStore&,const LocalEconomyState&,const PublicReliefCatalog&,PublicReliefState&,const TaskGovernanceRead&,const GovernanceAuthorityRead&,const StableId& task_id);
// 历史连续性由S6真实治理读取确认；缺服务不能把历史批准冒充当前结算许可。
Outcome<GovernanceSettlementAttempt> settle_public_relief(CommonStore&,const LocalEconomyState&,PublicReliefState&,const TaskGovernanceRead&,const GovernanceAuthorityRead&,const StableId& task_id);
// 显式接管上下文与实际原收据同候选提交；原入口保旧profile路径。
Outcome<GovernanceSettlementAttempt> settle_public_relief(CommonStore&,const LocalEconomyState&,PublicReliefState&,const TaskGovernanceRead&,const GovernanceAuthorityRead&,const StableId& task_id,EconomicTakeoverPhaseContext* takeover);
// 已有外层WorldBundle候选/Common保存点专用；失败须一并撤回本域状态和共同写集。
Outcome<GovernanceSettlementAttempt> settle_public_relief_in_candidate(CommonStore&,const LocalEconomyState&,PublicReliefState&,const TaskGovernanceRead&,const GovernanceAuthorityRead&,const StableId& task_id);
// 显式接管上下文与实际原收据同候选提交；原入口保旧profile路径。
Outcome<GovernanceSettlementAttempt> settle_public_relief_in_candidate(CommonStore&,const LocalEconomyState&,PublicReliefState&,const TaskGovernanceRead&,const GovernanceAuthorityRead&,const StableId& task_id,EconomicTakeoverPhaseContext* takeover);
Status release_public_relief(CommonStore&,PublicReliefState&,const TaskGovernanceRead&,const StableId& task_id,const DynamicStepInput* phase=nullptr);
// 已有外层WorldBundle候选/Common保存点专用；失败须一并撤回本域状态和共同写集。
Status release_public_relief_in_candidate(CommonStore&,PublicReliefState&,const TaskGovernanceRead&,const StableId& task_id,const DynamicStepInput* phase=nullptr);
Status validate_public_relief(const CommonStore&,const LocalEconomyState&,const PublicReliefCatalog&,const PublicReliefState&,const TaskGovernanceRead&,const GovernanceAuthorityRead&);
Status validate_public_relief(const CommonStore&,const LocalEconomyState&,const PublicReliefCatalog&,const PublicReliefState&,const TaskGovernanceRead&,const GovernanceAuthorityRead&,const EconomicTakeoverReadContext*,const EconomicTakeoverExecutionState*);
// 接管前纯读认领原合同预约：true只证明真实责任及精确原预约，绝不授权发放或释放。
// false仅表示该ref并非本域合同；已认领但来源损坏必须返回error。
Outcome<bool> read_relief_public_wallet_reservation(const CommonStore&,const LocalEconomyState&,const PublicReliefCatalog&,const PublicReliefState&,const TaskGovernanceRead&,const GovernanceAuthorityRead&,const Owner& original_owner,const StableId& wallet_id,const Reservation&);
Outcome<bool> read_relief_public_lot_reservation(const CommonStore&,const LocalEconomyState&,const PublicReliefCatalog&,const PublicReliefState&,const TaskGovernanceRead&,const GovernanceAuthorityRead&,const Owner& original_owner,const StableId& inventory_id,const StableId& lot_id,const ReservationRef&);
// 原半日候选可显式读刚完成的to_tick劳动；不传phase的冷读仍只到Common.tick。
Outcome<bool> read_relief_public_wallet_reservation(const CommonStore&,const LocalEconomyState&,const PublicReliefCatalog&,const PublicReliefState&,const TaskGovernanceRead&,const GovernanceAuthorityRead&,const Owner& original_owner,const StableId& wallet_id,const Reservation&,const DynamicStepInput*);
Outcome<bool> read_relief_public_lot_reservation(const CommonStore&,const LocalEconomyState&,const PublicReliefCatalog&,const PublicReliefState&,const TaskGovernanceRead&,const GovernanceAuthorityRead&,const Owner& original_owner,const StableId& inventory_id,const StableId& lot_id,const ReservationRef&,const DynamicStepInput*);
// 重复占领只借前次真实原责任容器链证明当前保管者；不授予分发执行权。
Outcome<bool> read_relief_public_wallet_reservation(const CommonStore&,const LocalEconomyState&,const PublicReliefCatalog&,const PublicReliefState&,const TaskGovernanceRead&,const GovernanceAuthorityRead&,const Owner& current_custodian,const StableId& wallet_id,const Reservation&,const DynamicStepInput*,const EconomicTakeoverReadContext*);
Outcome<bool> read_relief_public_lot_reservation(const CommonStore&,const LocalEconomyState&,const PublicReliefCatalog&,const PublicReliefState&,const TaskGovernanceRead&,const GovernanceAuthorityRead&,const Owner& current_custodian,const StableId& inventory_id,const StableId& lot_id,const ReservationRef&,const DynamicStepInput*,const EconomicTakeoverReadContext*);
// 结果仅承认同一共同事实与本域实际收据；未知ID返回空，坏来源返回错误。
class PublicReliefRead final:public PublicReliefResultRead {
public:
    PublicReliefRead(const CommonStore&,const PublicReliefState&);
    Outcome<std::optional<PublicReliefReceipt>> public_relief_receipt(const StableId&)const override;
private:
    const CommonStore& common_;const PublicReliefState& state_;
};
Outcome<PublicReliefCatalog> decode_public_relief_catalog(const BoundaryValue&);
class ProjectionWriter;
Status emit_public_relief_state_fields(ProjectionWriter&,const PublicReliefState&);
BoundaryValue encode_public_relief_state(const PublicReliefState&);
Outcome<PublicReliefState> decode_public_relief_state(const BoundaryValue&);
Status visit_public_relief_checkpoint(CodecVisitor&,PublicReliefState&);
}
