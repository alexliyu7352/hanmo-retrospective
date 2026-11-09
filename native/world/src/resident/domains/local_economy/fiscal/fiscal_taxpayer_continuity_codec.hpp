// 独立段只保存暂停原因与真实原周期引用，复用原严格字段访问器和共同checkpoint编码。
#pragma once
#include "fiscal_taxpayer_continuity.hpp"
#include "fiscal_settlement_codec.hpp"
namespace hanmo::resident::dynamic_economy_codec {
template<class A> void record(A& a,FiscalTaxpayerSuspendedPeriod& v){a("period_id",v.period_id);a("district_id",v.district_id);a("policy_source_fact_id",v.policy_source_fact_id);a("due_tick",v.due_tick);a("blocked_source_ids",v.blocked_source_ids);}
template<class A> void record(A& a,FiscalTaxpayerContinuityState& v){a("version",v.version);a("initialized_tick",v.initialized_tick);a("last_step_tick",v.last_step_tick);a("suspended_periods",v.suspended_periods);}
}
