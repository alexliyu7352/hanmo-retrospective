// 同一原writer的有限键索引；只用于组合碰撞/缺payload门，不能替代记录值/source验证。
#pragma once
#include "projection_writer.hpp"
namespace hanmo::resident {
class ProjectionKeyIndex {
public:
    using Key=std::basic_string<char,std::char_traits<char>,CheckpointAllocator<char>>;
    explicit ProjectionKeyIndex(std::shared_ptr<CheckpointMemoryResource>);
    ProjectionKeyIndex(const ProjectionKeyIndex&)=delete;
    ProjectionKeyIndex& operator=(const ProjectionKeyIndex&)=delete;
    ProjectionKeyIndex(ProjectionKeyIndex&&)noexcept=default;
    ProjectionKeyIndex& operator=(ProjectionKeyIndex&&)noexcept=default;
    bool contains(std::string_view)const noexcept;
    Status insert_unique(std::string_view,std::string_view duplicate_code="DuplicateDomainPayload");
    Status merge_unique(const ProjectionKeyIndex&);
    void set(std::string_view);
    const CheckpointVector<Key>& rows()const noexcept{return keys_;}
    Status validate_order(const FieldOrder&)const;
private:
    std::shared_ptr<CheckpointMemoryResource> memory_;
    CheckpointVector<Key> keys_;
};
// 捕获只保原根和三个原组合字典的行ID，所有嵌套值仍由shape sink完整遍历。
class ProjectionCompositionIndex final:public ProjectionObservation {
public:
    explicit ProjectionCompositionIndex(std::shared_ptr<CheckpointMemoryResource>);
    ProjectionKeyIndex roots,action_params,facts,battles;
    Status field_seen(std::size_t,std::string_view,std::string_view)override;
};
}
