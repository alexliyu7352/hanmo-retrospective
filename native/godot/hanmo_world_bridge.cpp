// Godot适配仅传精确字节与复制结果；所有库存语义留在同一hanmo_world库。
#include "hanmo_world_bridge.hpp"
#include "bridge_identity.hpp"
#include "actor_commitments_result.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <exception>
#include <cstring>

namespace {
static_assert(sizeof(void*)==8 && sizeof(godot::Vector2)==8, "Bridge requires 64-bit pointers and single-precision real_t");
// 仅表示Godot返回类型无法无损承载原字符串，不改变原生查询接受域。
struct UnrepresentableString {};
// Godot带长度UTF8构造仍截断NUL；明确拒绝并逐字节往返核验，不能静默清洗ID或错误路径。
godot::String text(const std::string& value) {
    if(value.find('\0')!=std::string::npos) throw UnrepresentableString{};
    const auto result=godot::String::utf8(value.data(), static_cast<int64_t>(value.size()));
    const auto encoded=result.utf8();
    if(static_cast<std::size_t>(encoded.length())!=value.size() ||
       (!value.empty() && std::memcmp(encoded.get_data(),value.data(),value.size())!=0)) throw UnrepresentableString{};
    return result;
}
// 失败返回原码与路径，没有counts或默认成功数值。
godot::Dictionary failure(const std::string& code, const std::string& path) {
    godot::Dictionary result;
    result["ok"]=false; result["code"]=text(code); result["path"]=text(path);
    return result;
}
// PackedByteArray按原始内存与长度复制，不经String解码/正规化。
std::string copy_bytes(const godot::PackedByteArray& bytes) {
    if(bytes.is_empty()) return {};
    return {reinterpret_cast<const char*>(bytes.ptr()), static_cast<std::size_t>(bytes.size())};
}
// 编译身份由同一CMake根据已核依赖与真实SDK生成，输入不能覆盖它。
godot::Dictionary build_identity(bool legacy=false) {
    godot::Dictionary result;
    result["profile"]=legacy?"offline_actor_commitments":"offline_inventory_count";
    result["source_commit"]=hanmo::bridge_build::source_commit;
    result["source_sha256"]=hanmo::bridge_build::source_sha256;
    result["core_source_sha256"]=hanmo::bridge_build::core_source_sha256;
    result["dependency_lock_sha256"]=hanmo::bridge_build::lock_sha256;
    result["godot_cpp_commit"]=hanmo::bridge_build::godot_cpp_commit;
    result["api_sha256"]=hanmo::bridge_build::api_sha256;
    result["trimmed_api_sha256"]=hanmo::bridge_build::trimmed_api_sha256;
    result["interface_sha256"]=hanmo::bridge_build::interface_sha256;
    result["profile_sha256"]=hanmo::bridge_build::profile_sha256;
    result["compiler"]=hanmo::bridge_build::compiler;
    result["target"]="Linux-x86_64"; result["cpp_standard"]="17";
    result["godot_target"]="template_debug"; result["cmake_configuration"]=hanmo::bridge_build::configuration;
    result["precision"]="single"; result["real_t_bits"]=32;
    result["pointer_bits"]=64; result["builtin_configuration"]="float_64";
    return result;
}
// 唯一纯Value访客保持Godot原生类型；所有深层字符串及字典键复用精确往返门。
godot::Variant godot_value(const hanmo::detail::Value& value) {
    return std::visit([](const auto& stored)->godot::Variant {
        using T=std::decay_t<decltype(stored)>;
        if constexpr(std::is_same_v<T,std::monostate>) return godot::Variant{};
        else if constexpr(std::is_same_v<T,bool>) return godot::Variant(stored);
        else if constexpr(std::is_same_v<T,std::int64_t>) return godot::Variant(static_cast<int64_t>(stored));
        else if constexpr(std::is_same_v<T,std::string>) return godot::Variant(text(stored));
        else if constexpr(std::is_same_v<T,hanmo::detail::Value::Array>) {
            godot::Array result;
            for(const auto& item:stored) result.append(godot_value(item));
            return result;
        } else {
            godot::Dictionary result;
            for(const auto& [key,item]:stored) result[text(key)]=godot_value(item);
            return result;
        }
    },value.data);
}
// 新口显式限制legacy profile，完整临时模型在候选store构造之前释放，不复制校验算法。
hanmo::Result<bool> legacy_profile_only(const std::string& bytes) {
    const auto checked=hanmo::detail::decode_legacy_artifact(bytes);
    return checked.ok?hanmo::Result<bool>{true,true,{}}:hanmo::Result<bool>{false,false,checked.error};
}
} // 适配辅助不持有可变全局状态。

// Variant参数确保Godot不能把字符串或整数数组静默转换为PackedByteArray。
void HanmoWorldBridge::_bind_methods() {
    godot::ClassDB::bind_method(godot::D_METHOD("install_artifact","bytes"), &HanmoWorldBridge::install_artifact);
    godot::ClassDB::bind_method(godot::D_METHOD("query_batch","bytes"), &HanmoWorldBridge::query_batch);
    godot::ClassDB::bind_method(godot::D_METHOD("audit"), &HanmoWorldBridge::audit);
    godot::ClassDB::bind_method(godot::D_METHOD("install_legacy_artifact","bytes"), &HanmoWorldBridge::install_legacy_artifact);
    godot::ClassDB::bind_method(godot::D_METHOD("query_actor_commitments","bytes"), &HanmoWorldBridge::query_actor_commitments);
    godot::ClassDB::bind_method(godot::D_METHOD("propose_travel_schedule","bytes"), &HanmoWorldBridge::propose_travel_schedule);
    godot::ClassDB::bind_method(godot::D_METHOD("propose_military_schedule","bytes"), &HanmoWorldBridge::propose_military_schedule);
}

// 类型检查后保原库错误优先级；超限输入不分配第二份巨大字节缓冲。
godot::Dictionary HanmoWorldBridge::install_artifact(const godot::Variant& value) {
    try {
        if(value.get_type()!=godot::Variant::PACKED_BYTE_ARRAY) return failure("ExpectedPackedByteArray","/arguments/bytes");
        if(!std::holds_alternative<std::monostate>(runtime_)) return failure("AlreadyInstalled","/runtime");
        const godot::PackedByteArray bytes=value;
        if(static_cast<std::size_t>(bytes.size())>hanmo::wire_limit) {
            return failure("WireSizeLimit","/artifact");
        }
        // 先在局部候选中完成原安装、审计和全部可抛返回值构造；异常候选自行析构。
        auto candidate=std::make_unique<hanmo::InventoryProjection>();
        const auto installed=candidate->install_once(copy_bytes(bytes));
        if(!installed.ok) return failure(installed.error.code,installed.error.path);
        const auto identity=candidate->audit();
        godot::Dictionary result;
        result["ok"]=true; result["input_sha256"]=text(identity.input_sha256);
        result["payload_sha256"]=text(identity.payload_sha256);
        // 返回值先准备，判别联合候选构造也在发布前；最终移动赋值编译期保证不抛。
        Runtime ready{std::move(candidate)};
        runtime_=std::move(ready);
        return result;
    } catch(const UnrepresentableString&) { return failure("BridgeUnrepresentableString","/bridge/string"); }
    catch(...) { return failure("NativeBridgeFailure","/bridge"); }
}

// 仅成功全批转成新Array/Dictionary；数量保持规范十进制字符串，避免浮点路径。
godot::Dictionary HanmoWorldBridge::query_batch(const godot::Variant& value) const {
    try {
        if(value.get_type()!=godot::Variant::PACKED_BYTE_ARRAY) return failure("ExpectedPackedByteArray","/arguments/bytes");
        if(std::holds_alternative<std::monostate>(runtime_)) return failure("NotInstalled","/runtime");
        const auto* runtime=std::get_if<V1Runtime>(&runtime_);
        if(!runtime) return failure("UnsupportedProjectionProfile","/runtime/profile");
        const godot::PackedByteArray bytes=value;
        if(static_cast<std::size_t>(bytes.size())>hanmo::wire_limit) {
            return failure("WireSizeLimit","/queries");
        }
        const auto wire=copy_bytes(bytes);
        const auto queried=(*runtime)->query_batch(wire);
        if(!queried.ok) return failure(queried.error.code,queried.error.path);
        godot::Array rows;
        for(const auto& row:queried.value) {
            godot::Dictionary item;
            item["query_id"]=text(row.query_id); item["count"]=text(std::to_string(row.count));
            rows.append(item);
        }
        godot::Dictionary result;
        result["ok"]=true; result["format"]="HM-NATIVE-INVENTORY-RESULT/1";
        result["input_sha256"]=text((*runtime)->audit().input_sha256);
        result["queries_sha256"]=text(hanmo::sha256(wire)); result["counts"]=rows;
        return result;
    } catch(const UnrepresentableString&) { return failure("BridgeUnrepresentableString","/bridge/string"); }
    catch(...) { return failure("NativeBridgeFailure","/bridge"); }
}

// 新工件先限profile并完整准备成功响应，最后发布唯一实例；不扩大旧install的接受域。
godot::Dictionary HanmoWorldBridge::install_legacy_artifact(const godot::Variant& value) {
    try {
        if(value.get_type()!=godot::Variant::PACKED_BYTE_ARRAY) return failure("ExpectedPackedByteArray","/arguments/bytes");
        if(!std::holds_alternative<std::monostate>(runtime_)) return failure("AlreadyInstalled","/runtime");
        const godot::PackedByteArray bytes=value;
        if(static_cast<std::size_t>(bytes.size())>hanmo::wire_limit) return failure("WireSizeLimit","/artifact");
        const auto wire=copy_bytes(bytes);
        const auto limited=legacy_profile_only(wire);
        if(!limited.ok) return failure(limited.error.code,limited.error.path);
        auto candidate=std::make_unique<hanmo::ProjectionStore>();
        const auto installed=candidate->install_once(wire);
        if(!installed.ok) return failure(installed.error.code,installed.error.path);
        const auto identity=candidate->audit();
        godot::Dictionary result;
        result["ok"]=true; result["input_sha256"]=text(identity.input_sha256); result["payload_sha256"]=text(identity.payload_sha256);
        Runtime ready{std::move(candidate)};
        runtime_=std::move(ready);
        return result;
    } catch(const UnrepresentableString&) { return failure("BridgeUnrepresentableString","/bridge/string"); }
    catch(...) { return failure("NativeBridgeFailure","/bridge"); }
}

// 只转换同库已完成的有限关联值，NIL/BOOL/INT保持类型，不经JSON浮点路径。
godot::Dictionary HanmoWorldBridge::query_actor_commitments(const godot::Variant& value) const {
    try {
        if(value.get_type()!=godot::Variant::PACKED_BYTE_ARRAY) return failure("ExpectedPackedByteArray","/arguments/bytes");
        if(std::holds_alternative<std::monostate>(runtime_)) return failure("NotInstalled","/runtime");
        const auto* runtime=std::get_if<LegacyRuntime>(&runtime_);
        if(!runtime) return failure("UnsupportedProjectionProfile","/runtime/profile");
        const godot::PackedByteArray bytes=value;
        if(static_cast<std::size_t>(bytes.size())>hanmo::wire_limit) return failure("WireSizeLimit","/queries");
        const auto wire=copy_bytes(bytes);
        const auto queried=(*runtime)->actor_commitments(wire);
        if(!queried.ok) return failure(queried.error.code,queried.error.path);
        const auto values=hanmo::detail::actor_commitments_value(queried.value);
        const auto rows=godot_value(values);
        godot::Dictionary result;
        result["ok"]=true; result["format"]="HM-GODOT-ACTOR-COMMITMENTS-RESULT/1";
        result["input_sha256"]=text((*runtime)->audit().input_sha256);
        result["queries_sha256"]=text(hanmo::sha256(wire)); result["results"]=rows;
        return result;
    } catch(const UnrepresentableString&) { return failure("BridgeUnrepresentableString","/bridge/string"); }
    catch(...) { return failure("NativeBridgeFailure","/bridge"); }
}

// 独立PackedByteArray同步口只调用同库纯算法，全批转换成功后才返回；原安装联合保持不动。
godot::Dictionary HanmoWorldBridge::propose_travel_schedule(const godot::Variant& value) const {
    try {
        if(value.get_type()!=godot::Variant::PACKED_BYTE_ARRAY) return failure("ExpectedPackedByteArray","/arguments/bytes");
        const godot::PackedByteArray bytes=value;
        if(static_cast<std::size_t>(bytes.size())>hanmo::wire_limit) return failure("WireSizeLimit","/travel_schedule");
        const auto proposed=hanmo::travel_schedule_proposals(copy_bytes(bytes));
        if(!proposed.ok) return failure(proposed.error.code,proposed.error.path);
        godot::Array rows;
        for(const auto& row:proposed.value.proposals) {
            godot::Dictionary params,item;
            params["destination"]=text(row.destination);
            item["actor_id"]=text(row.actor_id); item["kind"]="travel"; item["params"]=params;
            item["next_index"]=static_cast<int64_t>(row.next_index); rows.append(item);
        }
        auto identity=build_identity();identity["profile"]="native_travel_schedule";
        godot::Dictionary result;
        result["ok"]=true; result["format"]="HM-NATIVE-TRAVEL-PROPOSALS/1";
        result["scope_token"]=text(proposed.value.scope_token);
        result["evaluated_count"]=static_cast<int64_t>(proposed.value.evaluated_count);
        result["proposals"]=rows; result["build"]=identity;
        return result;
    } catch(const UnrepresentableString&) { return failure("BridgeUnrepresentableString","/bridge/string"); }
    catch(...) { return failure("NativeBridgeFailure","/bridge"); }
}

// 新同步口只转换同库完整纯值批次；复用唯一Value转换并保持旧runtime_及audit不变。
godot::Dictionary HanmoWorldBridge::propose_military_schedule(const godot::Variant& value) const {
    try {
        if(value.get_type()!=godot::Variant::PACKED_BYTE_ARRAY) return failure("ExpectedPackedByteArray","/arguments/bytes");
        const godot::PackedByteArray bytes=value;
        if(static_cast<std::size_t>(bytes.size())>hanmo::wire_limit) return failure("WireSizeLimit","/military_schedule");
        const auto proposed=hanmo::military_schedule_proposals(copy_bytes(bytes));
        if(!proposed.ok) return failure(proposed.error.code,proposed.error.path);
        godot::Array rows;
        for(const auto& row:proposed.value.proposals) {
            godot::Dictionary item;
            item["actor_id"]=text(row.actor_id); item["kind"]=text(row.kind);
            item["params"]=godot_value(hanmo::detail::Value{row.params}); rows.append(item);
        }
        auto identity=build_identity(); identity["profile"]="native_military_schedule";
        godot::Dictionary result;
        result["ok"]=true; result["format"]="HM-NATIVE-MILITARY-PROPOSALS/1";
        result["scope_token"]=text(proposed.value.scope_token);
        result["evaluated_count"]=static_cast<int64_t>(proposed.value.evaluated_count);
        result["proposals"]=rows; result["build"]=identity;
        return result;
    } catch(const UnrepresentableString&) { return failure("BridgeUnrepresentableString","/bridge/string"); }
    catch(...) { return failure("NativeBridgeFailure","/bridge"); }
}

// 审计容器逐层重建，释放RefCounted时成员的RAII直接释放唯一原生实例。
godot::Dictionary HanmoWorldBridge::audit() const {
    try {
        const auto* legacy=std::get_if<LegacyRuntime>(&runtime_);
        const auto* v1=std::get_if<V1Runtime>(&runtime_);
        const auto native=legacy?(*legacy)->audit():(v1?(*v1)->audit():hanmo::ProjectionAudit{});
        godot::Dictionary lots,components;
        for(const auto& [id,values]:native.lot_ids) {
            godot::Array row;
            for(const auto& value:values) row.append(text(value));
            lots[text(id)]=row;
        }
        for(const auto& [id,value]:native.component_sha256) components[text(id)]=text(value);
        godot::Dictionary result;
        result["ok"]=true; result["installed"]=native.installed;
        result["install_count"]=static_cast<int64_t>(native.install_count);
        result["input_sha256"]=text(native.input_sha256); result["payload_sha256"]=text(native.payload_sha256);
        result["inventory_lot_ids"]=lots; result["inventory_component_sha256"]=components;
        result["build"]=build_identity(legacy!=nullptr);
        if(legacy) {
            godot::Dictionary domains;
            for(const auto& [domain,records]:native.record_component_sha256) {
                godot::Dictionary rows;
                for(const auto& [id,digest]:records) rows[text(id)]=text(digest);
                domains[text(domain)]=rows;
            }
            result["profile"]=text(native.profile); result["record_component_sha256"]=domains;
        }
        return result;
    } catch(const UnrepresentableString&) { return failure("BridgeUnrepresentableString","/bridge/string"); }
    catch(...) { return failure("NativeBridgeFailure","/bridge"); }
}
