#pragma once
#include "read_stamp.hpp"
namespace hanmo::resident {
struct WorldBundle;
// 仅本次真实WorldBundle的栈借用，不缓存Moment，不提供capture或外部setter。
class WorldCurrentMomentRead final:public CurrentMomentRead {
public:
    explicit WorldCurrentMomentRead(const WorldBundle& world):world_(world){}
    Outcome<WorldMoment> read_current_moment()const override;
private:
    const WorldBundle& world_;
};
struct ActiveReadPhase:CheckpointAllocatedObject {CheckpointMemoryCharge memory;StableId phase,provider;MomentSpan span;WorldMoment actual_moment;std::uint64_t generation=0; ActiveReadPhase(CheckpointMemoryCharge&& m,StableId p,StableId owner,const MomentSpan& s,const WorldMoment& actual,std::uint64_t g):memory(std::move(m)),phase(std::move(p)),provider(std::move(owner)),span(s),actual_moment(actual),generation(g){} };
// 仅S0实际compiled回调栈持有；离开、异常退出均失效已发票据。
class PhaseCurrentReadScope:public CheckpointAllocatedObject {
public:
    ~PhaseCurrentReadScope();
    PhaseCurrentReadScope(const PhaseCurrentReadScope&)=delete;
    PhaseCurrentReadScope& operator=(const PhaseCurrentReadScope&)=delete;
private:
    friend struct WorldBundle;
    PhaseCurrentReadScope(const WorldBundle& world,std::uint64_t generation):world_(world),generation_(generation){}
    const WorldBundle& world_;std::uint64_t generation_;
};
class WorldCurrentReadStampRead final:public CurrentReadStampRead,public CheckpointAllocatedObject {
public:
    // 仅绑定实际根和显式actor/来源清单；限额取真实phase与输入文本，仍由原capture收费。
    static Outcome<std::shared_ptr<const ReadStamp>> capture_binding(const WorldBundle&,const DomainReadVersionSource&,const std::vector<ResourceKey>&,const std::vector<StableId>& actual_sources={});
    Outcome<WorldMoment> read_current_moment()const override;
    Outcome<std::shared_ptr<const ReadStamp>> capture(const std::vector<ResourceKey>&,
        const std::vector<DomainReadKey>&,const std::vector<StableId>&)const override;
private:
    friend struct WorldBundle;
    WorldCurrentReadStampRead(const WorldBundle&,const DomainReadVersionSource&,ReadStampLimits);
    const WorldBundle& world_;const DomainReadVersionSource& versions_;ReadStampLimits limits_;
};
}
