// C8.3 PIN-1：派生引用索引，不保存可写refcount，也不产生资产处分权限。
#pragma once
#include "source_ref.hpp"
#include "read_stamp.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include <functional>
#include <initializer_list>
#include <string_view>
namespace hanmo::resident {
struct WorldBundle;
struct ConsumerRef {StableId domain,kind,id,slot;};
enum class SourcePinReason {active_obligation,owned_asset,active_authority,dependent_state,unique_outcome};
struct SourcePin {ConsumerRef consumer;SourceRef source;SourcePinReason reason=SourcePinReason::dependent_state;};
// Borrow actual source/consumer pieces; no owning temporary is needed to emit a pin.
struct SourcePinBuildView {
    std::string_view consumer_domain,consumer_kind,consumer_id,consumer_slot;
    std::string_view source_domain,source_kind,source_id;
    SourcePinReason reason=SourcePinReason::dependent_state;
};
// Original consumer IDs sometimes concatenate borrowed row/ordinal pieces.
// This temporary owns the one final-size string before a sink takes its
// independently charged copy; callers retain their established ID format.
class SourcePinConsumerId {
    CheckpointMemoryCharge memory_;
    StableId value_;
    SourcePinConsumerId(std::initializer_list<std::string_view>,std::size_t);
public:
    explicit SourcePinConsumerId(std::initializer_list<std::string_view>);
    SourcePinConsumerId(const SourcePinConsumerId&)=delete;
    SourcePinConsumerId& operator=(const SourcePinConsumerId&)=delete;
    SourcePinConsumerId(SourcePinConsumerId&&)noexcept=default;
    SourcePinConsumerId& operator=(SourcePinConsumerId&&)=delete;
    const StableId& value()const noexcept{return value_;}
};
class SourcePinSink {
public:
    virtual ~SourcePinSink()=default;
    virtual Status append(const SourcePinBuildView&)=0;
    // Original key identity excludes reason. Domain-local dedup keeps the first reason.
    virtual bool contains(const SourcePinBuildView&)const noexcept=0;
};
struct RetentionSourceKind {StableId domain,kind;};
struct RequiredSourceConsumerSlot {StableId domain,kind,slot;};
// 由编译owner声明有限slot与实际枚举，不接外部pin=true/自由释放条件。
struct SourceConsumerRegistration {
    StableId domain,kind,slot;std::size_t max_references_per_consumer=0;
    std::vector<RetentionSourceKind> allowed_source_kinds;
    std::function<Status(const WorldBundle&,SourcePinSink&)> enumerate_actual_pins;
    std::function<Status(const WorldBundle&,const SourcePin&)> validate_actual_pin;
};
struct SourceRetentionRegistration {
    RetentionSourceKind source_kind;
    std::function<Status(const WorldBundle&,const SourceRef&)> validate_actual_source;
    std::function<Outcome<bool>(const WorldBundle&,const SourceRef&)> owner_may_release;
};
struct SourceRetentionRegistry {
    // 从当前PreparedCatalog锁定的compiled消费者清单取得，不能由业务请求裁掉依赖。
    std::vector<RequiredSourceConsumerSlot> required_consumer_slots;
    std::vector<SourceConsumerRegistration> consumers;
    std::vector<SourceRetentionRegistration> sources;
};
class SourcePinCollector;
// 本栈拥有的派生索引；任何对应writer/rollback后须重新枚举，绝不持久化。
// These outputs are read-only after collection. Payload charge is destroyed last;
// tracked vector shells carry their own actual allocation owner through moves.
class DerivedSourcePins {
    CheckpointMemoryCharge payload_memory_{0};
    friend class SourcePinCollector;
public:
    CheckpointVector<SourcePin> pins;
    DerivedSourcePins()=default;
    DerivedSourcePins(const DerivedSourcePins&)=delete;
    DerivedSourcePins& operator=(const DerivedSourcePins&)=delete;
    DerivedSourcePins(DerivedSourcePins&&)noexcept=default;
    DerivedSourcePins& operator=(DerivedSourcePins&& other)noexcept{swap(other);return *this;}
    void swap(DerivedSourcePins& other)noexcept{pins.swap(other.pins);payload_memory_.swap(other.payload_memory_);}
};
class SourceReleaseCandidates {
    CheckpointMemoryCharge payload_memory_{0};
    friend Outcome<SourceReleaseCandidates> prepare_source_release_candidates(const SourceRetentionRegistry&,const WorldBundle&,const std::vector<SourceRef>&);
public:
    CheckpointVector<SourceRef> sources;
    SourceReleaseCandidates()=default;
    SourceReleaseCandidates(const SourceReleaseCandidates&)=delete;
    SourceReleaseCandidates& operator=(const SourceReleaseCandidates&)=delete;
    SourceReleaseCandidates(SourceReleaseCandidates&&)noexcept=default;
    SourceReleaseCandidates& operator=(SourceReleaseCandidates&& other)noexcept{swap(other);return *this;}
    void swap(SourceReleaseCandidates& other)noexcept{sources.swap(other.sources);payload_memory_.swap(other.payload_memory_);}
};
Status validate_source_retention_registry(const SourceRetentionRegistry&);
Outcome<DerivedSourcePins> collect_actual_source_pins(const SourceRetentionRegistry&,const WorldBundle&);
Outcome<bool> source_may_release(const SourceRetentionRegistry&,const WorldBundle&,const SourceRef&);
Outcome<SourceReleaseCandidates> prepare_source_release_candidates(const SourceRetentionRegistry&,const WorldBundle&,const std::vector<SourceRef>& owner_candidates);
}
