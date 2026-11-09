#include "checkpoint_domain_list.hpp"
namespace hanmo::resident {
CheckpointDomainList::CheckpointDomainList(const std::vector<CheckpointDomain>& base){
    // V§9最多256目录；一次fresh reserve不发生几何增容，有限字符串每次构造前另收费。
    if(base.size()>256)throw std::bad_alloc();memory_.grow(0);values_.reserve(256);
    for(const auto& row:base)push_back(row);
}
CheckpointDomainList& CheckpointDomainList::operator=(CheckpointDomainList&& other)noexcept{if(this!=&other){this->~CheckpointDomainList();new(this)CheckpointDomainList(std::move(other));}return *this;}
void CheckpointDomainList::push_back(CheckpointDomainView value){
    if(values_.size()==256||value.name.size()>256)throw std::bad_alloc();
    // 锁定工程STL的fresh range构造，SSO不分配；没有先造临时std::string再补账。
      (memory_).grow(0);
    values_.push_back({StableId(value.name.data(),value.name.size()),value.codec_version});
}
}
