#include "common_fact_prefix.hpp"
#include "resident/kernel/world_stage_observation.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {
Outcome<std::size_t> read_actual_common_fact_position(const CommonState& common,const SourceRef& source){
    const auto* header=common.facts.find(source.id);
    if(!header||header->id!=source.id||header->domain!=source.domain||header->kind!=source.kind)
        return {false,0,common_failure("CommonActualFactHeaderRequired",source.id).error};
    return read_actual_common_fact_position(common,*header);
}
Outcome<std::size_t> common_fact_prefix_detail::read_borrowed_position(const CommonState& common,const SourceRefBuildView& source){
    // Borrow the stored key/header. The same original stored-header reader
    // below proves index identity and sole append position; no parsed ID order.
    const FactRecord* header=nullptr;
    for(const auto& row:common.facts.rows())if(row.first==source.id){header=&row.second;break;}
    if(!header||header->id!=source.id||header->domain!=source.domain||header->kind!=source.kind)
        return {false,0,common_failure("CommonActualFactHeaderRequired",StableId(source.id)).error};
    return read_actual_common_fact_position(common,*header);
}
Outcome<std::size_t> common_fact_prefix_detail::read_borrowed_position(const CommonState& common,const FactRecord& header){
    if(header.id.empty()||header.domain.empty()||header.kind.empty()||
       common.facts.find(header.id)!=&header)
        return {false,0,common_failure("CommonActualFactHeaderRequired",header.id).error};
    const auto& order=common.facts.order();
    const auto at=std::find(order.begin(),order.end(),header.id);
    if(at==order.end()||std::find(at+1,order.end(),header.id)!=order.end())
        return {false,0,common_failure("CommonActualFactOrderRequired",header.id).error};
    world_stage_observation::observe_node({world_stage_observation::NodeMetric::resolved_entry, "common.header.position", &common, nullptr, {"common_header", header.domain, header.kind, header.id, 0}, {}, world_stage_observation::observed_cut(static_cast<std::size_t>(at-order.begin()))});
    return {true,static_cast<std::size_t>(at-order.begin()),{}};
}
Outcome<std::size_t> read_actual_common_fact_position(const CommonStore& common,const SourceRef& source){
    return read_actual_common_fact_position(common.state(),source);
}
Outcome<std::size_t> common_fact_prefix_detail::read_borrowed_position(const CommonStore& common,const SourceRefBuildView& source){
    return read_actual_common_fact_position(common.state(),source);
}
Outcome<std::size_t> common_fact_prefix_detail::read_borrowed_position(const CommonStore& common,const FactRecord& header){
    return read_actual_common_fact_position(common.state(),header);
}
namespace {
template<class Anchor>
Outcome<std::size_t> read_prefix_count(
    const CommonStore& common,Integer fact_count,const Anchor* anchor){
    if(fact_count<0)
        return {false,0,common_failure("CommonActualFactPrefixMismatch").error};
    const auto& ids=common.state().facts.order();
    const auto count=static_cast<std::uint64_t>(fact_count);
    if(count>ids.size()||bool(count)!=bool(anchor))
        return {false,0,common_failure("CommonActualFactPrefixMismatch").error};
    const auto limit=static_cast<std::size_t>(count);
    if(count){
        const auto& last=*anchor;
        if(ids[limit-1]!=last.id)
            return {false,0,common_failure("CommonActualFactPrefixMismatch",last.id).error};
        auto position=read_actual_common_fact_position(common,last);
        if(!position.ok)return {false,0,std::move(position.error)};
        if(position.value+1!=limit)
            return {false,0,common_failure("CommonActualFactPrefixMismatch",last.id).error};
    }
    return {true,limit,{}};
}
}
Outcome<std::size_t> read_actual_common_fact_prefix(
    const CommonStore& common,const CommonActualFactPrefix& prefix){
    return read_prefix_count(common,prefix.common_fact_count,
        prefix.last_common_fact?&*prefix.last_common_fact:nullptr);
}
Outcome<std::size_t> read_actual_common_fact_prefix(
    const CommonStore& common,const CurrentCommonFactPrefix& prefix){
    return read_prefix_count(common,prefix.common_fact_count,prefix.last_common_fact);
}
Outcome<CurrentCommonFactPrefix> read_current_common_fact_prefix(const CommonStore& common){
    const auto& ids=common.state().facts.order();
    if(ids.size()>static_cast<std::size_t>(std::numeric_limits<Integer>::max()))
        return {false,{},common_failure("CommonCurrentFactPrefixOverflow").error};
    CurrentCommonFactPrefix prefix;prefix.common_fact_count=static_cast<Integer>(ids.size());
    if(!ids.empty()){
        prefix.last_common_fact=common.state().facts.find(ids.back());
        if(!prefix.last_common_fact||prefix.last_common_fact->id!=ids.back())
            return {false,{},common_failure("CommonCurrentFactPrefixMismatch").error};
    }
    return {true,prefix,{}};
}
Outcome<bool> actual_common_fact_in_prefix(const CommonStore& common,const SourceRef& source,const CommonActualFactPrefix& prefix){
    auto limit=read_actual_common_fact_prefix(common,prefix);if(!limit.ok)return {false,false,std::move(limit.error)};
    auto position=read_actual_common_fact_position(common,source);if(!position.ok)return {false,false,std::move(position.error)};
    return {true,position.value<limit.value,{}};
}
namespace {
template<class Source>
Outcome<CurrentCommonFactPrefix> prefix_before(const CommonStore& common,const Source& source){
    auto position=read_actual_common_fact_position(common,source);
    if(!position.ok)return {false,{},std::move(position.error)};
    if(position.value>static_cast<std::size_t>(std::numeric_limits<Integer>::max()))
        return {false,{},common_failure("CommonCurrentFactPrefixOverflow").error};
    CurrentCommonFactPrefix prefix;
    prefix.common_fact_count=static_cast<Integer>(position.value);
    if(position.value){
        const auto& previous=common.state().facts.order()[position.value-1];
        prefix.last_common_fact=common.state().facts.find(previous);
        if(!prefix.last_common_fact||prefix.last_common_fact->id!=previous)
            return {false,{},common_failure("CommonActualFactPrefixMismatch",previous).error};
    }
    auto checked=read_actual_common_fact_prefix(common,prefix);
    if(!checked.ok)return {false,{},std::move(checked.error)};
    return {true,prefix,{}};
}
}
Outcome<CurrentCommonFactPrefix> read_actual_common_fact_prefix_before(const CommonStore& common,const SourceRef& source){
    return prefix_before(common,source);
}
Outcome<CurrentCommonFactPrefix> common_fact_prefix_detail::read_borrowed_prefix_before(const CommonStore& common,const SourceRefBuildView& source){
    return prefix_before(common,source);
}
Outcome<CurrentCommonFactPrefix> common_fact_prefix_detail::read_borrowed_prefix_before(const CommonStore& common,const FactRecord& source){
    return prefix_before(common,source);
}
namespace {
template<class Prefix>
Status visit_prefix(const CommonStore& common,const Prefix& prefix,CommonActualFactPrefixSink& sink){
    auto limit=read_actual_common_fact_prefix(common,prefix);if(!limit.ok)return {false,{},std::move(limit.error)};
    const auto& order=common.state().facts.order();
    for(std::size_t i=0;i<limit.value;++i){
        const auto* header=common.state().facts.find(order[i]);
        if(!header)return common_failure("CommonActualFactHeaderRequired",order[i]);
        auto position=read_actual_common_fact_position(common,*header);
        if(!position.ok)return {false,{},std::move(position.error)};
        if(position.value!=i)return common_failure("CommonActualFactOrderRequired",order[i]);
        auto valid=sink.actual_prefix_fact(i,*header);if(!valid.ok)return valid;
    }
    return common_success();
}
}
Status visit_actual_common_fact_prefix(const CommonStore& common,const CommonActualFactPrefix& prefix,
    CommonActualFactPrefixSink& sink){
    return visit_prefix(common,prefix,sink);
}
Status visit_actual_common_fact_prefix(const CommonStore& common,const CurrentCommonFactPrefix& prefix,
    CommonActualFactPrefixSink& sink){
    return visit_prefix(common,prefix,sink);
}
}
