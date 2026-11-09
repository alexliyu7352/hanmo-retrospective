// 持续事项只拥有来源、共享责任及验收历史；实际钱货仍由CommonStore持有。
#pragma once
#include "resident/domains/tasks/task_model.hpp"
#include <set>
#include "state_need.hpp"
namespace hanmo::resident {
struct EconomicSupplySuspension;class EconomicSupplyPolicyRead;
struct StateNeedCatalog;
struct CohortWageNeedCatalog;
class CohortWageNeedValidationRead;
struct FiscalNeedCatalog;
class FiscalNeedValidationRead;
enum class NeedMeasure { stock_gap,cumulative_delivery,outstanding_obligation };
enum class NeedStatus { open,partial,settling,fulfilled,expired,cancelled };
enum class AffairPhase { open,active,suspended,resolved,failed,cancelled };
enum class ClaimStatus { accepted,in_progress,settled,released,expired };
struct WorldAffair { StableId id,semantic_key;Integer episode=1;SubjectRef principal;std::vector<StableId> cause_fact_ids;AffairPhase phase=AffairPhase::open;Integer opened_tick=0;std::optional<Integer> closed_tick;std::vector<StableId> need_ids; };
// physical_amount只是最后一次登记观察的快照；stock_gap绝不再减历史fulfilled_amount。
struct Need { StableId id,affair_id,source_domain,source_key;SubjectRef beneficiary;StableId target_inventory_id,item_id,unit;NeedMeasure measure=NeedMeasure::stock_gap;Integer revision=1,created_tick=0;std::optional<Integer> deadline_tick;Integer target_amount=0,fulfilled_amount=0;NeedStatus status=NeedStatus::open;std::vector<StableId> receipt_ids,claim_ids;StableId source_fact_id,policy_id;Integer physical_amount=0,source_revision=0,last_observed_tick=0;StableId obligation_id,arrear_id; };
struct NeedClaim { StableId id,need_id;SubjectRef assignee;Integer accepted_amount=0,fulfilled_amount=0,accepted_tick=0,expires_tick=0;ClaimStatus status=ClaimStatus::accepted;StableId contract_id;std::optional<NeedClaimAcceptanceSource> accepted_source; };
struct ContributionUse { StableId receipt_id,transfer_fact_id,contract_id,claim_id,need_id;Integer counted_amount=0; };
// policy以精确来源键连接源域和不可变任务模板；支持的验收类型显式登记。
struct NeedPolicy { StableId id,source_domain="local_economy",source_key,semantic_key,organization_id,site_id,inventory_id,item_id,unit="unit",template_id,terms_version="1",funding_wallet_id,reward_wallet_id;NeedMeasure measure=NeedMeasure::stock_gap;Integer max_claim_quantity=0,purchase_budget_per_unit=0,reward_per_unit=0,deadline_ticks=0;StableId accept_role,report_role,cancellation_policy_id="return_actual_cargo_v1";std::optional<Integer> reward_total; };
// 欠款需求只引用唯一真实义务/欠款，不创建可花余额或采购合同。
struct ObligationNeedPolicy { StableId id,obligation_id,organization_id,semantic_key; };
struct AffairsCatalog { OrderedTable<NeedPolicy> policies;bool obligation_needs=false;OrderedTable<ObligationNeedPolicy> obligation_policies;std::shared_ptr<const FiscalNeedCatalog> fiscal_need_policies;std::shared_ptr<const CohortWageNeedCatalog> cohort_wage_need_policies;std::shared_ptr<const StateNeedCatalog> state_need_policies; };
struct AffairsState { bool exact_claims=false;std::optional<StateNeedState> state_needs; OrderedTable<WorldAffair> affairs;OrderedTable<Need> needs;OrderedTable<NeedClaim> claims;OrderedTable<ContributionUse> contributions;bool obligation_needs=false;bool fiscal_needs=false;bool cohort_wage_needs=false; };
struct NeedClaimQuote { StableId need_id;Integer need_revision=0,amount=0;SubjectRef assignee;StableId executor_actor_id,offer_id;FrozenTaskTerms terms;std::vector<ReadVersion> resource_versions;std::optional<ActingAuthority> authority; };
struct NeedClaimReceipt { StableId need_id,claim_id,contract_id,task_id;Integer accepted_amount=0; };
struct OpportunityView { OfferRef offer;StableId need_id,affair_id,item_id,unit;Integer need_revision=0,available_amount=0; };
Outcome<AffairsCatalog> decode_affairs_catalog(const BoundaryValue&);
Outcome<AffairsState> decode_affairs_state(const BoundaryValue&,bool obligation_needs=false,bool fiscal_needs=false,bool cohort_wage_needs=false);
BoundaryValue encode_affairs_state(const AffairsState&);
Status visit_affairs_checkpoint(CodecVisitor&,AffairsState&,bool obligation_needs=false,bool fiscal_needs=false,bool cohort_wage_needs=false);
Outcome<AffairsState> make_initial_affairs(const AffairsCatalog&);
Integer need_gap(const Need&);
Outcome<Integer> need_available_amount(const AffairsState&,const Need&);
Outcome<NeedClaimQuote> preview_need_claim(const CommonStore&,const AffairsState&,const TaskState&,const KnownSubjectView&,const StableId& need_id,Integer amount,const EconomicSupplyPolicyRead* issued_supply=nullptr);
// 有明确执行者的底层报价供中央与个人任务共同采用，known校验留在调用入口。
Outcome<NeedClaimQuote> prepare_need_claim(const CommonStore&,const AffairsState&,const TaskState&,const StableId& need_id,Integer amount,const SubjectRef&,const StableId& executor_actor_id,const StableId& offer_id,Integer expected_revision,const OrganizationRead* organizations=nullptr,const ActingAuthority* authority=nullptr,const StableId& payee_wallet_id={},const EconomicSupplyPolicyRead* issued_supply=nullptr);
Outcome<NeedClaimReceipt> claim_need(CommonStore&,AffairsState&,TaskState&,const NeedClaimQuote&,const OrganizationRead* organizations=nullptr,const EconomicSupplyPolicyRead* issued_supply=nullptr);
Status apply_contribution(AffairsState&,const TaskState&,const DeliveryFact&);
Status release_claim(AffairsState&,const StableId& claim_id,ClaimStatus reason);
Status reconcile_affairs(CommonStore&,AffairsState&,const AffairsCatalog&,TaskState&,const std::vector<SupplyObservation>&,const DynamicResultRead&,Integer tick,const TaskExecutionServices* execution=nullptr,const std::vector<EconomicSupplySuspension>* supply_suspensions=nullptr,const std::set<StableId>* issued_suspended_policy_ids=nullptr,const DynamicStepInput* original_calendar_phase=nullptr);
// 模板仅在首次发布时借用；已发布实例及已签合同保持条款，不回读可变库存作冻结量。
Status publish_need_offers(CommonStore&,AffairsState&,const AffairsCatalog&,TaskState&,const TaskCatalog&,Integer tick);
Status validate_affairs(const CommonStore&,const AffairsState&,const AffairsCatalog&,const TaskState&,const DynamicResultRead* results=nullptr,const FiscalNeedValidationRead* fiscal_needs=nullptr,const CohortWageNeedValidationRead* cohort_wages=nullptr,const std::vector<EconomicSupplySuspension>* supply_suspensions=nullptr,const std::set<StableId>* issued_suspended_policy_ids=nullptr,const WorldMoment* actual_moment=nullptr);
std::vector<OpportunityView> query_opportunities(const AffairsState&,const TaskState&,const KnownSubjectView&);
} // 每次认领和交付均随同一候选提交；只读页面不会建立事项或占用份额。
