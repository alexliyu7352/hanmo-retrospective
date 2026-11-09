// 新政治接管JSON入口先核完整字段树，拒绝first-match解码可能忽略的另一份权源。
#pragma once
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/storage/common_codec.hpp"
#include <set>
namespace hanmo::resident {
// 只应用于接管catalog/command/独立存档段，不改变旧政治模块或固定二进制字段序。
inline Status validate_political_takeover_payload(const BoundaryValue& value,std::size_t depth=0){
    if(depth>CodecVisitor::max_depth)return common_failure("PoliticalTakeoverPayloadTooDeep");
    if(const auto* fields=value.as<BoundaryValue::Dict>()){
        if(fields->size()>CodecVisitor::max_items)return common_failure("PoliticalTakeoverPayloadTooLarge");
        std::set<StableId> names;
        for(const auto& field:*fields){
            if(!names.insert(field.first).second)return common_failure("DuplicatePoliticalTakeoverField",field.first);
            auto valid=validate_political_takeover_payload(field.second,depth+1);if(!valid.ok)return valid;
        }
    }
    if(const auto* rows=value.as<BoundaryValue::Array>()){
        if(rows->size()>CodecVisitor::max_items)return common_failure("PoliticalTakeoverPayloadTooLarge");
        for(const auto& row:*rows){auto valid=validate_political_takeover_payload(row,depth+1);if(!valid.ok)return valid;}
    }
    return common_success();
}
}
