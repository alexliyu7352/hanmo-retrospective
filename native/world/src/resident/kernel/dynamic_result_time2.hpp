// T固定后继的深拥有结果读取；只读快照不授予当前候选的执行权，也不拥有第二事实账。
#pragma once
#include "resident/model/dynamic_contract.hpp"
#include "world_timeline.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
namespace hanmo::resident {
struct WorldBundle;struct FactRecord;class CommonStore;struct LocalEconomyState;struct GrowthState;
// 原整数源只能明确保其边界精度；精确源必须反读原owner真实已应用字段。
struct LegacyFactBoundary { Integer original_tick=0,original_source_schema=0; };
struct OwnerFactMoment { SourceRef source;Integer source_schema=0; };
using FactTimeBasis=std::variant<LegacyFactBoundary,OwnerFactMoment>;
// 字段序沿T§2.1；日历供给/义务内部字段不机械改成Moment。
struct DynamicFactViewV2 {
 StableId id,kind,rule_version;WorldMoment occurred_at;FactTimeBasis time_basis;
 std::vector<StableId> cause_fact_ids,affair_ids;std::vector<FactParticipant> participants;
 std::optional<SupplyObservation> supply;std::optional<ObligationObservation> obligation;
 StableId subject_id,outcome;bool publicly_visible=false;
};
// 只有原Action终局才返回；工时满不自动宣告业务成功。
struct DynamicActionResultV2 {
 StableId action_id,action_kind,source_fact_id,code;WorldMoment completed_at;FactTimeBasis time_basis;
 bool success=false;std::vector<StableId> participant_actor_ids;
};
struct ExactResultReadIdentity;
// Static owner dispatch returns an explicit disposition for every selected original header.
// A non-reportable header is not an empty Fact and cannot be queried as a successful absent result.
struct ExactOwnerFactRead {
 // 租约先于深值声明；采集移动期间两端保活同一实际 payload。
 CheckpointCopyLease payload_memory;
 enum class Disposition { readable, no_dynamic_projection };
 Disposition disposition=Disposition::readable;
 std::optional<DynamicFactViewV2> fact;
 std::optional<DynamicActionResultV2> action;
 bool reportable=false;std::vector<SubjectRef> direct_observers;
 // Immutable original sources without public DynamicFact payload still remain directly readable.
 std::vector<MomentOwnerSource> supporting_sources;
};
class ExactResultOwnerRead {
public:
 virtual ~ExactResultOwnerRead()=default;
 virtual Outcome<ExactOwnerFactRead> read_selected_fact(const FactRecord&)const=0;
 virtual Outcome<WorldMoment> read_selected_source_moment(const MomentOwnerSource&)const=0;
};
// 本片仅签发不可变history快照票，不冒通用phase-current或可写授权ReadStamp。
class ExactResultReadStamp {
 friend class ExactResultTimeRead;
 std::weak_ptr<const ExactResultReadIdentity> owner_;
};
// 游标只绑定同一深拥有快照与原order锚，不能跨world/revision/范围续页。
struct ExactFactCursor {
 ExactResultReadStamp stamp;
 std::optional<StableId> last_scanned_fact_id,through_fact_id;
 std::optional<WorldMoment> after,through;
};
struct ExactFactPage {std::vector<StableId> ids;std::optional<StableId> last_scanned_fact_id;std::optional<ExactFactCursor> next;bool complete=false;};
// 完整选择沿具名原owner派发；未知或未接齐的真实provider拒绝，不降成整数源。
class ExactResultTimeRead final {
 struct Row {CheckpointCopyLease payload_memory;StableId id;std::optional<DynamicFactViewV2> fact;std::optional<DynamicActionResultV2> action;bool no_dynamic_projection=false,reportable=false;std::vector<SubjectRef> direct_observers;};
 struct SourceMoment {MomentOwnerSource source;WorldMoment at;};
 CheckpointMemoryCharge object_memory_;
 WorldIdentity identity_;WorldMoment through_;CheckpointVector<Row> rows_;
 CheckpointVector<SourceMoment> source_moments_;
 std::shared_ptr<const ExactResultReadIdentity> owner_;
 explicit ExactResultTimeRead(CheckpointMemoryCharge memory):object_memory_(std::move(memory)){}
 Status check(const ExactResultReadStamp&)const;
 friend Outcome<std::shared_ptr<const ExactResultTimeRead>> capture_exact_result_time_read(const WorldBundle&);
 friend Outcome<std::shared_ptr<const ExactResultTimeRead>> capture_exact_result_time_read(const CommonStore&,const LocalEconomyState&,const GrowthState&,const WorldTimelineState&);
 friend Outcome<std::shared_ptr<const ExactResultTimeRead>> capture_exact_result_time_read(const CommonStore&,const WorldTimelineState&,const ExactResultOwnerRead&,const std::vector<MomentOwnerSource>&);
 friend Outcome<std::shared_ptr<const ExactResultTimeRead>> capture_exact_result_time_read(const CommonStore&,const WorldTimelineState&,const ExactResultOwnerRead&,const std::vector<MomentOwnerSource>&,const WorldMoment&);
public:
 ExactResultTimeRead(const ExactResultTimeRead&)=delete;ExactResultTimeRead& operator=(const ExactResultTimeRead&)=delete;
 // 只发本不可变快照的history读票；获取票不运行采集、发信或业务写口。
 ExactResultReadStamp stamp()const;
 Status validate_history_stamp(const ExactResultReadStamp& stamp)const{return check(stamp);}
 const WorldIdentity& source_identity()const{return identity_;}
 const WorldMoment& source_moment()const{return through_;}
 std::optional<StableId> last_fact_id()const{return rows_.empty()?std::nullopt:std::optional<StableId>{rows_.back().id};}
 // Borrowed values remain valid only while this immutable snapshot owner is retained.
 Outcome<const DynamicFactViewV2*> fact_v2_borrowed(const StableId&,const ExactResultReadStamp&)const;
 Outcome<const DynamicActionResultV2*> action_result_v2_borrowed(const StableId&,const ExactResultReadStamp&)const;
 Outcome<const std::vector<SubjectRef>*> direct_fact_observers_borrowed(const StableId&,const ExactResultReadStamp&)const;
 Outcome<std::optional<DynamicFactViewV2>> fact_v2(const StableId&,const ExactResultReadStamp&)const;
 Outcome<std::optional<DynamicActionResultV2>> action_result_v2(const StableId&,const ExactResultReadStamp&)const;
 Outcome<bool> reportable_fact(const StableId&,const ExactResultReadStamp&)const;
 Outcome<std::vector<SubjectRef>> direct_fact_observers(const StableId&,const ExactResultReadStamp&)const;
 Outcome<WorldMoment> source_moment_v2(const MomentOwnerSource&,const ExactResultReadStamp&)const;
 Outcome<ExactFactPage> fact_ids_after_source_cursor(const std::optional<StableId>& last,const std::optional<StableId>& through,Integer limit,const ExactResultReadStamp&)const;
 Outcome<ExactFactPage> fact_ids_between_moments(const WorldMoment& after,const WorldMoment& through,Integer limit,const std::optional<ExactFactCursor>&,const ExactResultReadStamp&)const;
};
// 原owner完整值在同一authority只读片中捕获；返回后不借用可变WorldBundle。
Outcome<std::shared_ptr<const ExactResultTimeRead>> capture_exact_result_time_read(const WorldBundle&);
// 旧bootstrap专用叶入口保留历史调用；完整WorldBundle入口不再使用该窄口。
Outcome<std::shared_ptr<const ExactResultTimeRead>> capture_exact_result_time_read(const CommonStore&,const LocalEconomyState&,const GrowthState&,const WorldTimelineState&);
// source refs without Common headers (communication, professional results) are captured from their unique owner.
Outcome<std::shared_ptr<const ExactResultTimeRead>> capture_exact_result_time_read(const CommonStore&,const WorldTimelineState&,const ExactResultOwnerRead&,const std::vector<MomentOwnerSource>& retained_sources);
// 原Report保旧字段字节；此投影只给其显示桶，精确来源由binding另核，不能作为新reader。
DynamicFactView legacy_report_projection(const DynamicFactViewV2&);
}

namespace hanmo::resident {
// Actual moment comes from the active S0 phase; Common Calendar is published only at slice end.
Outcome<std::shared_ptr<const ExactResultTimeRead>> capture_exact_result_time_read(const CommonStore&,const WorldTimelineState&,const ExactResultOwnerRead&,const std::vector<MomentOwnerSource>&,const WorldMoment& actual_phase_moment);
}
