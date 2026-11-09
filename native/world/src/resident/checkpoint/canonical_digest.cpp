// 从已实现growth叶原样提取排序、共同JSON输出与SHA256流程；所有领域复用同一字节算法。
#include "canonical_digest.hpp"
#include "resident/storage/common_codec.hpp"
#include <picosha2.h>
#include <algorithm>
namespace hanmo::resident {
    namespace {
        // 字典按原稳定字符串键排序；数组顺序与所有标量类型原样保留。
        void sort_value(BoundaryValue& x){if(auto* rows=x.as<BoundaryValue::Dict>()){std::sort(rows->begin(),rows->end(),[](const auto& a,const auto& b){return a.first<b.first;
                });
                for(auto& row:*rows)sort_value(row.second);
            }else if(auto* values=x.as<BoundaryValue::Array>())for(auto& value:*values)sort_value(value);
        }
    }
    // 输入先复制，后续排序不变更作者目录、报价或调用方缓存。
    StableId boundary_canonical_digest(const BoundaryValue& source){BoundaryValue value=source;
        sort_value(value);
        auto json=render_boundary_json(value);
        if(!json.ok)boundary::fail(json.error.original.code,json.error.original.path);
        return picosha2::hash256_hex_string(json.value);
    }
}
