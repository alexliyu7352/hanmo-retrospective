// 原profile目录选择的有限拥有结果；只改分配边界，版本/顺序仍由原world selector决定。
#pragma once
#include "checkpoint_envelope.hpp"
#include "checkpoint_copy_shape.hpp"
#include <string_view>
namespace hanmo::resident {
struct CheckpointDomainView {
    std::string_view name;Integer codec_version;
    CheckpointDomainView(std::string_view n,Integer v):name(n),codec_version(v){}
    CheckpointDomainView(const CheckpointDomain& value):name(value.name),codec_version(value.codec_version){}
};
class CheckpointDomainList {
public:
    explicit CheckpointDomainList(const std::vector<CheckpointDomain>&);
    CheckpointDomainList(const CheckpointDomainList&)=delete;
    CheckpointDomainList& operator=(const CheckpointDomainList&)=delete;
    CheckpointDomainList(CheckpointDomainList&&)noexcept=default;
    CheckpointDomainList& operator=(CheckpointDomainList&&)noexcept;
    void push_back(CheckpointDomainView);
    auto begin()noexcept{return values_.begin();}auto end()noexcept{return values_.end();}
    auto begin()const noexcept{return values_.begin();}auto end()const noexcept{return values_.end();}
    std::size_t size()const noexcept{return values_.size();}
    const std::vector<CheckpointDomain>& values()const noexcept{return values_;}
    operator const std::vector<CheckpointDomain>&()const noexcept{return values_;}
private:
    CheckpointMemoryCharge memory_;
    std::vector<CheckpointDomain> values_;
};
}
