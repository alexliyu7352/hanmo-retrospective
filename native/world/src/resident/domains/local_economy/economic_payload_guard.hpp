// 新经济独立冷段先核完整字段树；不让first-match解码吞掉同名的另一份权源。
#pragma once
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/checkpoint/checkpoint_json_decode.hpp"
namespace hanmo::resident {
// 仅用于明确的新段JSON准入，二进制仍沿原固定CodecVisitor，不改变旧LE载荷。
inline Status economic_payload_failure(const char* code,std::string_view path={}){
    if(ScopedCheckpointJsonDecode::current()||ScopedCheckpointBoundaryDecode::current())checkpoint_decode_boundary_failure(code,{path},ErrorKind::rule);
    return common_failure(code,StableId{path});
}
inline Status validate_economic_payload_fields(const BoundaryValue& value,std::size_t depth=0){
    if(depth>CodecVisitor::max_depth)return economic_payload_failure("EconomicPayloadTooDeep");
    if(const auto* fields=value.as<BoundaryValue::Dict>()){
        if(fields->size()>CodecVisitor::max_items)return economic_payload_failure("EconomicPayloadTooLarge");
        for(std::size_t index=0;index<fields->size();++index){
            const auto& field=(*fields)[index];
            for(std::size_t prior=0;prior<index;++prior)if((*fields)[prior].first==field.first)return economic_payload_failure("DuplicateEconomicPayloadField",field.first);
            auto valid=validate_economic_payload_fields(field.second,depth+1);if(!valid.ok)return valid;
        }
    }
    if(const auto* rows=value.as<BoundaryValue::Array>()){
        if(rows->size()>CodecVisitor::max_items)return economic_payload_failure("EconomicPayloadTooLarge");
        for(const auto& row:*rows){auto valid=validate_economic_payload_fields(row,depth+1);if(!valid.ok)return valid;}
    }
    return common_success();
}
}
