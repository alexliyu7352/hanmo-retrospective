#pragma once
#include "repair_service.hpp"
#include "repair_service_codec.hpp"
#include "resident/domains/local_economy/service_finance_execution.hpp"
#include "resident/domains/craft/repair_policy.hpp"
#include "resident/domains/craft/repair_service_sources.hpp"
#include "resident/domains/craft/repair_detail.hpp"
#include "resident/storage/resource_execution_draft.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident::repair_service_detail {
struct Failure{RuleError error;};
[[noreturn]]inline void fail(const char*code,const StableId&id={},ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,id,kind).error};}
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
inline int compare(const WorldMoment&a,const WorldMoment&b){return take(compare_world_moments(a,b));}
inline bool same(const MomentOwnerSource&a,const MomentOwnerSource&b){return a.source_schema==b.source_schema&&same_source_ref(a.source_ref,b.source_ref);}
inline bool owner(const Owner&a,const Owner&b){return !a.extra&&!b.extra&&a.kind==b.kind&&a.id==b.id;}
inline bool subject(const SubjectRef&a,const SubjectRef&b){return a.kind==b.kind&&a.id==b.id;}
inline Integer add(Integer a,Integer b){if(a<0||b<0||a>std::numeric_limits<Integer>::max()-b)fail("RepairServiceAmountOverflow");return a+b;}
inline void ref(const MomentOwnerSource&r){if(r.source_schema<1||!valid_source_ref(r.source_ref))fail("RepairServiceOriginalSourceRequired",r.source_ref.id);}
inline std::size_t header(const CommonStore&c,const SourceRef&r){const auto*h=c.state().facts.find(r.id);if(!h||h->id!=r.id||h->kind!=r.kind||h->domain!=r.domain)fail("RepairServiceOriginalHeaderRequired",r.id);const auto&order=c.state().facts.order();auto it=std::find(order.begin(),order.end(),r.id);if(it==order.end())fail("RepairServiceOriginalHeaderRequired",r.id);return static_cast<std::size_t>(it-order.begin());}
inline WorldMoment now(const RepairServiceReadContext&x){auto at=take(x.clock.read_current_moment());take(compare_world_moments(at,at));if(at.half_day_tick<x.common.tick())fail("RepairServiceClockBeforeCommon");return at;}
inline void context(const RepairServiceReadContext&x){if(!x.original.bound_to(x.common,x.items))fail("RepairServiceContextMismatch");now(x);if(x.repair&&(&x.repair->common!=&x.common||&x.repair->items!=&x.items||&x.repair->policies!=&x.policies||&x.repair->clock!=&x.clock))fail("RepairServiceS9ContextMismatch");}
inline void writable(CommonStore&c,ItemInstancesState&i,const RepairServiceReadContext&x){context(x);if(&c!=&x.common||&i!=&x.items)fail("RepairServiceCandidateMismatch");}
inline void writable(CommonStore&c,ItemInstancesState&i,LocalEconomyState&e,const RepairServiceReadContext&x){writable(c,i,x);if(&e!=&x.economy)fail("RepairServiceFinancialCandidateMismatch");}
inline void append(CommonStore&c,const SourceRef&r){take(c.append_fact({r.id,r.kind,r.domain}));}
inline const RepairServiceContractsState&state(const RepairServiceReadContext&x){return x.items.repair_service_contracts;}
void contract_shape(const RepairServiceReadContext&,const RepairServiceContractTerms&,const WorldMoment&,const RepairServiceContractSource*);
const RepairAcceptedSource& accepted_scope(const RepairServiceReadContext&,const RepairServiceContractSource&);
WorldMoment actual_trigger(const RepairServiceReadContext&,const RepairServiceContractSource&,const MomentOwnerSource&);
const RepairServiceContractSource&contract(const RepairServiceReadContext&,const MomentOwnerSource&);
const RepairServiceChargeSource&charge_original_source(const RepairServiceReadContext&,const MomentOwnerSource&);
const RepairServiceChargeSource&charge(const RepairServiceReadContext&,const MomentOwnerSource&);
const RepairServicePaymentSource&payment(const RepairServiceReadContext&,const MomentOwnerSource&);
using AppliedRepairMoneyLegs=service_finance::AppliedMoney;
AppliedRepairMoneyLegs apply_original_repair_money(CommonStore&,const StableId& payer,const StableId& payee,Integer amount);
}
namespace hanmo::resident::repair_service_detail {
const RepairServiceTerminationSettlementSource&settlement(const RepairServiceReadContext&,const MomentOwnerSource&);
WorldMoment terminal_moment(const RepairServiceReadContext&,const RepairServiceContractSource&,const MomentOwnerSource&);
void validate_repair_settlement_sources(const RepairServiceReadContext&);
}
