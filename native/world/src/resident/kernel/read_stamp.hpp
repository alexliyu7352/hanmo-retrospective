// C5.2热借用合同：不持久化，不提供业务权限，只有候选组合根可以发行。
#pragma once
#include "hanmo/resident/world_moment.hpp"
#include <memory>
#include "hanmo/resident/checkpoint_memory.hpp"
#include <initializer_list>
#include <vector>
namespace hanmo::resident {
class CheckpointRetentionCensus;
struct WorldBundle;
struct ReadStampState;
enum class ReadMode { committed_current, step_opening, phase_current, committed_history };
struct DomainReadVersion { StableId domain,kind,id; std::uint64_t revision=0; };
struct DomainReadKey { StableId domain,kind,id; };
struct ReadStampLimits { std::size_t max_versions=0,max_sources=0,max_text_bytes=0; };
// 只借本次调用输入；兼容普通和同job allocator数组，不在入参处复制。
template<class T>class ReadStampRange {
public:
    ReadStampRange()noexcept=default;
    template<class A>ReadStampRange(const std::vector<T,A>& value)noexcept:data_(value.data()),count_(value.size()){}
    ReadStampRange(std::initializer_list<T> value)noexcept:data_(value.begin()),count_(value.size()){}
    const T* begin()const noexcept{return data_;}const T* end()const noexcept{return count_?data_+count_:data_;}
    std::size_t size()const noexcept{return count_;}
private:const T* data_=nullptr;std::size_t count_=0;
};
struct ReadStamp {
public:
    void retain_checkpoint_memory(CheckpointRetentionCensus&)const;
    const WorldIdentity& identity() const noexcept {return identity_;}
    ReadMode mode() const noexcept {return mode_;}
    const StableId& phase() const noexcept {return phase_;}
    const MomentSpan& span() const noexcept {return span_;}
    const CheckpointVector<ReadVersion>& resources() const noexcept {return resources_;}
    const CheckpointVector<DomainReadVersion>& domains() const noexcept {return domains_;}
    const CheckpointVector<StableId>& source_ids() const noexcept {return sources_;}
private:
    friend class ReadStampAuthority;
    ReadStamp()=default;
    CheckpointMemoryCharge text_memory_;
    std::shared_ptr<CheckpointAllocationOwner> candidate_allocation_;
    std::weak_ptr<const ReadStampState> candidate_;
    std::uint64_t slice_=0,savepoint_=0;
    WorldIdentity identity_;
    ReadMode mode_=ReadMode::committed_current;
    StableId phase_;
    MomentSpan span_;
    CheckpointVector<ReadVersion> resources_;
    CheckpointVector<DomainReadVersion> domains_;
    CheckpointVector<StableId> sources_;
};
class DomainReadVersionSource {
public:
    virtual ~DomainReadVersionSource()=default;
    virtual Outcome<std::uint64_t> current_version(const StableId& domain,const StableId& kind,const StableId& id) const=0;
};
// 业务只声明所读对象；身份、真实phase/片段和当前版本由组合根取得。
// 只读实际时刻不授予stamp发行能力；只要时刻的consumer无需伪造版本源。
class CurrentMomentRead {
public:
    virtual ~CurrentMomentRead()=default;
    virtual Outcome<WorldMoment> read_current_moment()const=0;
};
class CurrentReadStampRead:public CurrentMomentRead {
public:
    ~CurrentReadStampRead() override=default;
    virtual Outcome<std::shared_ptr<const ReadStamp>> capture(
        const std::vector<ResourceKey>&,const std::vector<DomainReadKey>&,
        const std::vector<StableId>& actual_source_ids)const=0;
};
class ReadStampAuthority:public CheckpointAllocatedObject {
public:
    ReadStampAuthority(const ReadStampAuthority&)=delete;
    ReadStampAuthority& operator=(const ReadStampAuthority&)=delete;
    ReadStampAuthority(ReadStampAuthority&&)=delete;
    ReadStampAuthority& operator=(ReadStampAuthority&&)=delete;
    ~ReadStampAuthority();
    void retain_checkpoint_memory(CheckpointRetentionCensus&)const;
    Outcome<std::shared_ptr<const ReadStamp>> issue(const WorldIdentity&,ReadMode,const StableId& phase,
        const MomentSpan&,ReadStampRange<ReadVersion>,ReadStampRange<DomainReadVersion>,ReadStampRange<StableId>,
        const ReadStampLimits&,ExactArithmeticBudget={}) const;
    Status validate(const ReadStamp&,const WorldIdentity&,ReadMode,const CommonRead&,const DomainReadVersionSource&) const;
    // 每次相关写入/保存点切换都失效旧slice；rollback也递增，绝不还原凭据。
    Status invalidate_after_write();
    Status invalidate_after_savepoint();
    void note_mutation_boundary() noexcept;
    Status usable() const;
    void retire() noexcept;
private:
    friend struct WorldBundle;
    ReadStampAuthority();
    std::shared_ptr<ReadStampState> state_;
};
// 原具名writer调用边界；包括拒绝/异常返回都废弃该slice，不替代业务回滚。
class ReadStampWriteBoundary {
public:
    explicit ReadStampWriteBoundary(ReadStampAuthority& owner) noexcept:owner_(owner){owner_.note_mutation_boundary();}
    ~ReadStampWriteBoundary(){owner_.note_mutation_boundary();}
    ReadStampWriteBoundary(const ReadStampWriteBoundary&)=delete;
    ReadStampWriteBoundary& operator=(const ReadStampWriteBoundary&)=delete;
private:
    ReadStampAuthority& owner_;
};
}
