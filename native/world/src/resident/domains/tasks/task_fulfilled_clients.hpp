// 首店资格只借本人真实Task履约史；原客户身份不随经办人、收件人或付款接管者变化。
#pragma once
#include "task_model.hpp"
#include <variant>
namespace hanmo::resident {
struct DynamicStepInput;
class CurrentMomentRead;
struct TaskPublicAccountExecutionState;
struct TaskStaticAssetState;
struct TaskMessageHandoffState;
// 每个原客户只给一份可复核成功来源；受益人另列，不额外增加客户数。
struct TaskFulfilledClientSource {
  Owner original_client;
  SubjectRef delivery_beneficiary;
  StableId task_id, settlement_id, template_id, payment_fact_id;
  Integer completed_tick = 0;
  std::vector<StableId> objective_fact_ids;
  // Source-local exact branch. completed_tick is only its compatibility
  // projection; historical eligibility comes from this original source and
  // the actual private_shop_opened Common cut, never from that projection.
  std::optional<WorldMoment> completed_at;
  std::optional<MomentOwnerSource> completion_ref;
};
struct TaskFulfilledClients {
  WorldIdentity source_identity;
  StableId actor_id;
  std::vector<TaskFulfilledClientSource> clients;
};
// 只接受同候选已完整验证的Task/资产/支付/交接段；缺旧凭据仅不计，不能补写历史。
struct TaskFulfilledClientContext {
  const CommonStore &common;
  const TaskState &tasks;
  const TaskCatalog &catalog;
  const CommonCatalog &common_catalog;
  const TaskPublicAccountExecutionState *payments = nullptr;
  const TaskStaticAssetState *static_assets = nullptr;
  const TaskMessageHandoffState *messages = nullptr;
  // 原完成半日尚未推钟时只扩全账观察上界；开店资格仍按其原事实截面。
  const DynamicStepInput *phase = nullptr;
  const CurrentMomentRead *actual_clock = nullptr;
};
// 历史边界仅认可实际private_shop_opened头，原S5调用方还须核该payload.tick。
struct TaskFulfilledClientBoundary {
  StableId opening_fact_id;
  Integer opening_tick = 0;
};
Outcome<TaskFulfilledClients> read_task_fulfilled_clients(
    const TaskFulfilledClientContext &, const StableId &actor_id,
    const TaskFulfilledClientBoundary *before = nullptr);
// 保留接口只借原已完整读取的实际根；callback 同步消费，不保存此短栈 context。
// 返回全部实际根而非选出的三客户，保留候选排除和后缀检查所借行；不授释放权。
struct TaskStaticPaymentSource;
struct TaskStaticAssetReceipt;
struct TaskMessageHandoff;
struct TaskMessageReport;
struct ExactQuantityActionCompletion;
struct TaskShopOrderBinding;
struct TaskShopOrderSettlement;
struct TaskShopOrderAction;
struct TaskShopOrderInterruption;
struct TaskShopOrderEstateCancellation;
struct TaskPersonalMessageStart;
struct TaskPersonalMessageEnd;
struct TaskPersonalReportStart;
struct TaskPersonalReportEnd;
struct TaskOriginalOrganizationClientProvenance {
  const CommonCatalog *catalog = nullptr;
  const StableId *actual_organization_id = nullptr;
};
struct TaskMessageObserverProvenance {
  const TaskMessageHandoffState *state = nullptr;
  const StableId *actual_observer_key = nullptr;
  const std::vector<StableId> *actual_index = nullptr;
  bool report = false;
};
// 有限实际行/原只读view；没有凭字符串猜 schema 的通用来源分支。
// ExactQuantityActionCompletion 与两个 Provenance 小view均是短栈借用；
// 接收方只能保存其内部指向真实根的指针，不能保存view自身地址。
using TaskFulfilledClientCheckedSource = std::variant<
    const TaskRecord *, const ContractRecord *, const TaskDefinition *,
    const TaskTransferFact *, const DeliveryFact *, const EscortArrivalFact *,
    const TaskStaticPaymentSource *, const TaskStaticAssetReceipt *,
    const TaskMessageHandoff *, const TaskMessageReport *,
    const ExactQuantityActionCompletion *, const TaskShopOrderBinding *,
    const TaskShopOrderSettlement *, const TaskShopOrderAction *,
    const TaskShopOrderInterruption *, const TaskShopOrderEstateCancellation *,
    const TaskPersonalMessageStart *, const TaskPersonalMessageEnd *,
    const TaskPersonalReportStart *, const TaskPersonalReportEnd *,
    const TaskMessageObserverProvenance *, const ActorRecord *,
    const TaskOriginalOrganizationClientProvenance *>;
class TaskFulfilledClientProvenanceSink {
public:
  virtual ~TaskFulfilledClientProvenanceSink() = default;
  // 与原分支同一次读取，部分输出必须等整体成功才可发布。
  virtual Status checked_source(const TaskFulfilledClientContext &,
                                const TaskFulfilledClientCheckedSource &) = 0;
  virtual Status actual_roots(const TaskFulfilledClientContext &,
                              const StableId &actor_id,
                              const TaskFulfilledClientBoundary &) = 0;
};
Status visit_task_fulfilled_clients_before_shop_provenance(
    const TaskFulfilledClientContext &, const StableId &actor_id,
    const TaskFulfilledClientBoundary &, TaskFulfilledClientProvenanceSink &);
// S5只借具名纯读，不获得Task全表或构造可写合同的能力。
class TaskFulfilledClientRead {
public:
  virtual ~TaskFulfilledClientRead() = default;
  // 未实现的旧借口明确不支持保留，绝不以空 provider 假装完整来源。
  virtual Status
  visit_before_shop_provenance(const StableId &, const StableId &, Integer,
                               TaskFulfilledClientProvenanceSink &) const {
    return common_failure("TaskClientProvenanceUnavailable", {},
                          ErrorKind::unsupported_capability);
  }
  virtual Outcome<TaskFulfilledClients>
  task_fulfilled_clients(const StableId &actor_id) const = 0;
  virtual Outcome<TaskFulfilledClients>
  task_fulfilled_clients_before_shop(const StableId &actor_id,
                                     const StableId &opening_fact_id,
                                     Integer opening_tick) const = 0;
};
class TaskFulfilledClientAccess final : public TaskFulfilledClientRead {
public:
  explicit TaskFulfilledClientAccess(TaskFulfilledClientContext context)
      : context_(context) {}
  Status visit_before_shop_provenance(
      const StableId &, const StableId &, Integer,
      TaskFulfilledClientProvenanceSink &) const override;
  Outcome<TaskFulfilledClients>
  task_fulfilled_clients(const StableId &actor_id) const override;
  Outcome<TaskFulfilledClients>
  task_fulfilled_clients_before_shop(const StableId &actor_id,
                                     const StableId &opening_fact_id,
                                     Integer opening_tick) const override;

private:
  TaskFulfilledClientContext context_;
};
} // namespace hanmo::resident
