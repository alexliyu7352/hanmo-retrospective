// 同库各离线profile共用的值类型；标签、所有权整值和原批次序只定义一次。
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace hanmo {
// 失败携带明确码和字段路径，调用方不得将默认值当成功结果。
struct Error { std::string code; std::string path; };
// 仅成功时value有效；失败不带部分查询结果。
template<class T> struct Result {
    bool ok=false;
    T value{};
    Error error{};
};
// 每条数量为旧有效域内整数，wire输出另按规范十进制编码。
struct CountRow { std::string query_id; std::int64_t count=0; };
// 审计只含来源身份与安装观察值，不提供可写组件或额外资产真值。
struct ProjectionAudit {
    bool installed=false;
    std::string profile;
    std::string input_sha256;
    std::string payload_sha256;
    std::size_t install_count=0;
    std::map<std::string,std::vector<std::string>> lot_ids;
    std::map<std::string,std::string> component_sha256;
    std::map<std::string,std::map<std::string,std::string>> record_component_sha256;
};
// 摘要只用于精确字节绑定，不是世界身份或可信验证证明。
std::string sha256(const std::string& bytes);
inline constexpr std::size_t wire_limit=64u*1024u*1024u;
inline constexpr std::size_t wire_depth_limit=64;
namespace detail {
// 类型标签参与相等；String数字不会与整数或布尔混淆。
struct Value {
    using Array=std::vector<Value>;
    using Dict=std::map<std::string,Value>;
    std::variant<std::monostate,bool,std::int64_t,std::string,Array,Dict> data;
    // 标签、数组次序和字典完整键值共同参与相等，不做数字类型转换。
    bool operator==(const Value& other) const { return data==other.data; }
};
// 批次额外键完整保留，原数组位置由InventoryState.lots唯一确定。
struct Lot {
    std::string id,item_id,reserved_by;
    std::int64_t quantity=0;
    Value owner;
    Value::Dict extras;
};
// 单个容器的原始只读组件，不保存可独立变化的可用量。
struct InventoryState {
    std::string id;
    Value owner;
    std::int64_t capacity=0,revision=0;
    std::vector<Lot> lots;
    Value::Dict extras;
};
} // 类型化值不提供写世界能力。
} // 公共结果与私有组件共用一份声明。
