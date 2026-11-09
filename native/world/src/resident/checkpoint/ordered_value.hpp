// 初始化、检查点与诊断专用的边界值；绝不能作为常驻世界的动态权威存储。
#pragma once
#include "hanmo/resident/model_contract.hpp"
#include <functional>
#include <type_traits>
#include <cstdint>
#include <limits>
#include <string_view>

namespace hanmo::resident {
// 每个数值标签和字典插入序均保留；领域所有者负责将这些输入解码为自己的typed状态。
struct BoundaryValue {
    using Array = std::vector<BoundaryValue>;
    using Dict = std::vector<std::pair<std::string, BoundaryValue>>;
    // Original alternatives keep their positions. Only integers above Int64 max
    // need the final unsigned alternative; this is a JSON boundary scalar,
    // never a second permanent world value or a new binary field tag.
    using Data = std::variant<std::monostate, bool, Integer, double, std::string, Array, Dict, std::uint64_t>;
    Data data;
    BoundaryValue() = default;
    // 显式保留既有值标签；旧标准库不得把字符串字面量优先转换为bool。
    BoundaryValue(Data value) : data(std::move(value)) {}
    // Preserve the original signed tag for the old nonnegative range. A named
    // uint64 schema can read both without accepting float, bool or negative input.
    template<class T, std::enable_if_t<std::is_integral_v<T> && std::is_unsigned_v<T> && !std::is_same_v<T,bool> && (sizeof(T)<=sizeof(std::uint64_t)),int> = 0>
    BoundaryValue(T value) : data(std::monostate{}) {
        const auto exact=static_cast<std::uint64_t>(value);
        if(exact<=static_cast<std::uint64_t>(std::numeric_limits<Integer>::max())) data=static_cast<Integer>(exact);
        else data=exact;
    }
    // 只接真正字符指针，不能把合法整数零抢作空指针。
    template<class T, std::enable_if_t<std::is_same_v<T,const char*>||std::is_same_v<T,char*>,int> = 0>
    BoundaryValue(T value) : data(std::in_place_type<std::string>, value) {}
    // 类型不符返回nullptr；调用者保留自身领域错误码与首错次序。
    template<class T> const T* as() const { return std::get_if<T>(&data); }
    template<class T> T* as() { return std::get_if<T>(&data); }
    // 缺键/非字典均返回nullptr；不会插入字段或提供默认值。
    const BoundaryValue* find(std::string_view key) const;
    BoundaryValue* find(std::string_view key);
};
// Strict JSON integers retain Int64 or the full nonnegative UInt64 range.
// Duplicate keys, values outside both ranges, nonfinite floats and limits reject.
Outcome<BoundaryValue> parse_boundary_json(const std::string&);
Outcome<std::string> render_boundary_json(const BoundaryValue&);
// 顺序只附在所属typed记录上；不按数组下标保存整世界路径拓扑。
Outcome<std::vector<StableId>> capture_field_order(const BoundaryValue& dictionary);
// 空顺序保留新记录writer字面序；非空必须是当前键的精确排列，拒绝丢字段/未知键。
Status apply_field_order(BoundaryValue& owned_dictionary,const std::vector<StableId>& saved_keys);
// 首个完整值/类型/数组差异和首个字典序差异分开，恢复旧codec时不能掩盖后者。
struct BoundaryDifference { std::string path, reason; };
struct BoundaryComparison {
    bool values_equal = true, dictionary_order_equal = true;
    std::optional<BoundaryDifference> value_difference, order_difference;
    bool exact() const { return values_equal && dictionary_order_equal; }
};
BoundaryComparison compare_boundary(const BoundaryValue&, const BoundaryValue&);
} // 这里只定义边界数据形状，不定义任何领域字段schema。
