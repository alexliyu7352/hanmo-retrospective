// 只为独立私铺工资定义真实本人接受；无预约/工时来源不能被当前余额冒结清。
#pragma once
#include "private_shop.hpp"
#include "private_shop_staff_contract.hpp"
#include "resident/domains/estate/employment/estate_employment.hpp"
namespace hanmo::resident {
struct PrivateShopOperationSourceContext;
class PrivateShopOperationSourcePinSink;
struct PrivateShopStaffContext {
    const PrivateShopContext& shops;const PrivateShopStaffCatalog& catalog;const PrivateShopStaffState& state;
    const EstateState& estates;const EstateEmploymentState* estate_employment=nullptr;const EstateAccess* estate_access=nullptr;
};
struct PrivateShopStaffQuote {WorldIdentity source_identity;StableId actor_id;PrivateShopStaffCommand command;PrivateShopStaffTerms terms;std::optional<AuthoredProviderSourceProof> agreement;};
struct PrivateShopStaffReceipt {StableId contract_id,fact_id,obligation_id;};
Status validate_private_shop_staff(const PrivateShopStaffContext&);
// Original accepted terms/assent/authority and terminal history dependency cut.
// It does not recurse into labor/wage results or claim today's active eligibility.
Outcome<const PrivateShopStaffContract*> read_original_private_shop_staff_contract(
 const PrivateShopContext&,const SourceRef& actual_acceptance);
Outcome<PrivateShopStaffQuote> preview_private_shop_staff(const PrivateShopStaffContext&,const PrivateShopMeetingContext&,const StableId& actor,const PrivateShopStaffCommand&);
Outcome<PrivateShopStaffReceipt> apply_private_shop_staff_in_candidate(CommonStore&,LocalEconomyState&,PrivateShopStaffState&,const PrivateShopStaffContext&,const PrivateShopMeetingContext&,const PrivateShopStaffQuote&);
// 到岗工资只在已预约的原营业Action内结算；外层Common/LE/Shop一起回滚。
Status reserve_private_shop_staff_day(CommonStore&,const PrivateShopContext&,const PrivateShopOperation&);
Status record_private_shop_staff_attendance(CommonStore&,LocalEconomyState&,PrivateShopState&,const PrivateShopContext&,const StableId& operation_id,Integer tick);
// Exact profile consumes the genuine S7 applied labor source, never an integer
// attendance substitution. The existing wage/remainder/payment engine is shared.
Outcome<std::optional<SourceRef>> record_private_shop_staff_exact_labor(CommonStore&,
 LocalEconomyState&,PrivateShopState&,const PrivateShopOperationSourceContext&,const SourceRef&);
Outcome<Integer> finish_private_shop_staff_day(CommonStore&,LocalEconomyState&,PrivateShopState&,const PrivateShopContext&,const StableId& operation_id,Integer tick);
Status validate_private_shop_staff_services(const PrivateShopContext&);
// 完整Family死亡账登记后调用；到期/真实死亡只终止将来服务，原当日工资先结清。
// 仅原已接受的NPC店员按明确商品继续到岗，玩家本人不会被自动派工；无货/占用/资金不足等待。
Status start_private_shop_npc_staff_work_in_candidate(CommonStore&,LocalEconomyState&,PrivateShopState&,const PrivateShopContext&);
Status finish_private_shop_staff_lifecycle_in_candidate(CommonStore&,LocalEconomyState&,PrivateShopState&,const PrivateShopContext&);
struct PrivateShopOperationSourcesState;struct PrivateShopOperationCapabilitiesState;
struct PrivateShopExactStaffTerminal {
 const PrivateShopStaffContract* contract=nullptr;
 const PrivateShopStaffTermination* decision=nullptr;
 WorldMoment occurred_at;
};
Outcome<PrivateShopExactStaffTerminal> read_private_shop_exact_staff_terminal(
 const PrivateShopOperationSourceContext&,const SourceRef& actual_staff_ended);
// Direct immutable dependencies of this actual original staff-ended row.
// Life4 death schema belongs to the death payload, not its cause schema.
Status visit_private_shop_exact_staff_terminal_pins(const PrivateShopOperationSourceContext&,
 const SourceRef& actual_staff_ended,PrivateShopOperationSourcePinSink&);
// Same original contract/decision writer, with actual exact lifecycle cause.
// Returns real operation DayResult IDs for same-candidate market reconciliation.
Outcome<std::vector<StableId>> finish_private_shop_staff_lifecycle_in_candidate(
 CommonStore&,LocalEconomyState&,PrivateShopState&,PrivateShopOperationSourcesState&,
 PrivateShopOperationCapabilitiesState&,ActionElapsedStore&,const PrivateShopOperationSourceContext&);
Outcome<StableId> interrupt_private_shop_exact_staff_operation_in_candidate(
 CommonStore&,LocalEconomyState&,PrivateShopState&,PrivateShopOperationSourcesState&,
 PrivateShopOperationCapabilitiesState&,ActionElapsedStore&,const PrivateShopOperationSourceContext&,
 const StableId& actual_operation,const SourceRef& actual_staff_ended);

// 仅供上述生命周期同候选调用；本口精确重核终止原因的当前真实来源。
Outcome<StableId> interrupt_private_shop_staff_operation_in_candidate(CommonStore&,LocalEconomyState&,PrivateShopState&,const PrivateShopContext&,const StableId& operation_id,const StableId& reason);
class PrivateShopStaffAccess final:public PrivateShopStaffRead {
public:
    explicit PrivateShopStaffAccess(const PrivateShopContext& context):context_(context){}
    Outcome<std::optional<PrivateShopStaffSource>> private_shop_staff_source(const StableId&)const override;
    Outcome<std::vector<StableId>> private_shop_worker_contracts(const StableId&)const override;
private:const PrivateShopContext& context_;
};
Outcome<StableId> private_shop_operation_staff(const PrivateShopContext&,const PrivateShopRecord&,const StableId& actor);
}

