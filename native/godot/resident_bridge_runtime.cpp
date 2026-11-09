// Godot对象转换只在调用主线程；worker仅运行普通C++session/typed世界，不碰Node或Variant。
#include "resident_bridge_runtime.hpp"
#include "resident/kernel/continuation_snapshot.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>

namespace {
using namespace hanmo::resident;
using Value=BoundaryValue;
using Clock=std::chrono::steady_clock;
// UTF8按精确字节转换，领域字符串仍由core在原guard位置验证。
std::string text(const godot::String& value) {const auto bytes=value.utf8(); return {bytes.get_data(),static_cast<std::size_t>(bytes.length())};}
godot::String text(const std::string& value) {return godot::String::utf8(value.data(),static_cast<int64_t>(value.size()));}
// 桥错误属于传输/生命周期；不重写任何领域错误码。
godot::Dictionary error(const std::string& code) {godot::Dictionary out; out["ok"]=false; out["code"]=text(code); return out;}
// 保留原能力/内部错误定位，不能把Unsupported伪装成普通游戏拒绝。
godot::Dictionary error(const RuleError& failure) {
    auto out=error(failure.original.code); out["path"]=text(failure.original.path);
    out["error_kind"]=failure.kind==ErrorKind::rule?"rule":failure.kind==ErrorKind::unsupported_capability?"unsupported_capability":"internal";
    out["phase"]=text(failure.phase); out["domain"]=text(failure.domain); out["command"]=text(failure.command); out["source_id"]=text(failure.source_id); return out;
}
// world_id逐字节编码，仅用于回调身份，不是可写world字段。
std::string world_hex(const WorldId& id) {constexpr char hex[]="0123456789abcdef"; std::string result; for(auto b:id) {result+=hex[b>>4];result+=hex[b&15];} return result;}
// 初态字节只允许PackedByteArray且有总上限，不进行隐式Variant到String转换。
[[maybe_unused]] std::string initial_bytes(const godot::Variant& value) {
    if(value.get_type()!=godot::Variant::PACKED_BYTE_ARRAY) throw std::invalid_argument("ResidentExpectedBytes");
    const godot::PackedByteArray bytes=value; if(bytes.size()>64*1024*1024) throw std::invalid_argument("ResidentInputTooLarge");
    return {reinterpret_cast<const char*>(bytes.ptr()),static_cast<std::size_t>(bytes.size())};
}
// 冷blob只接PackedByteArray并立即深复制；空blob的存在位也必须保留给core验证。
Bytes checkpoint_bytes(const godot::Variant& value,std::size_t limit=live_checkpoint_byte_limit) {
    if(value.get_type()!=godot::Variant::PACKED_BYTE_ARRAY)throw std::invalid_argument("ResidentExpectedBytes");
    const godot::PackedByteArray input=value;if(static_cast<std::size_t>(input.size())>limit)throw std::invalid_argument("ResidentInputTooLarge");
    Bytes result(static_cast<std::size_t>(input.size()));if(!result.empty())std::copy_n(input.ptr(),result.size(),result.data());return result;
}
// 冷结果在主线程变为独立Godot字节数组；永不经String、UTF8或逐字节JSON值。
godot::PackedByteArray packed_checkpoint(const Bytes& value) {
    if(value.size()>live_checkpoint_byte_limit)throw std::invalid_argument("ResidentOutputTooLarge");
    godot::PackedByteArray result;if(result.resize(static_cast<int64_t>(value.size()))!=0)throw std::runtime_error("ResidentOutputAllocationFailed");
    if(!value.empty())std::copy(value.begin(),value.end(),result.ptrw());return result;
}
// 小命令保bool/int64/binary64区别和Dictionary迭代序；不导入Godot对象指针。
Value from_variant(const godot::Variant& input,std::size_t depth,std::size_t& budget) {
    if(depth>64||budget==0) throw std::invalid_argument("ResidentCommandLimit");
    --budget;
    switch(input.get_type()) {
    case godot::Variant::NIL:return {std::monostate{}};
    case godot::Variant::BOOL:return {static_cast<bool>(input)};
    case godot::Variant::INT:return {static_cast<Integer>(input)};
    case godot::Variant::FLOAT: {double value=input; if(!std::isfinite(value)) throw std::invalid_argument("ResidentNonFiniteValue"); return {value};}
    case godot::Variant::STRING: {auto value=text(static_cast<godot::String>(input)); if(value.size()>budget) throw std::invalid_argument("ResidentCommandLimit"); budget-=value.size(); return {std::move(value)};}
    case godot::Variant::ARRAY: {godot::Array source=input; Value::Array value; if(static_cast<std::size_t>(source.size())>budget) throw std::invalid_argument("ResidentCommandLimit"); value.reserve(source.size()); for(int64_t i=0;i<source.size();++i) value.push_back(from_variant(source[i],depth+1,budget)); return {std::move(value)};}
    case godot::Variant::DICTIONARY: {godot::Dictionary source=input; const godot::Array keys=source.keys(); Value::Dict value; std::set<std::string> converted_keys;
        if(static_cast<std::size_t>(keys.size())>budget) throw std::invalid_argument("ResidentCommandLimit");
        // Godot同文String/StringName键是同一项；仅规范键文本，保留原序与原值类型，转换碰撞拒绝而不覆盖。
        for(int64_t i=0;i<keys.size();++i) {
            const auto key_type=keys[i].get_type();
            if(key_type!=godot::Variant::STRING&&key_type!=godot::Variant::STRING_NAME) throw std::invalid_argument("ResidentExpectedStringKey");
            const auto key=text(static_cast<godot::String>(keys[i]));
            if(key.size()>budget) throw std::invalid_argument("ResidentCommandLimit");
            budget-=key.size();
            if(!converted_keys.insert(key).second) throw std::invalid_argument("DuplicateBoundaryKey");
            value.emplace_back(key,from_variant(source[keys[i]],depth+1,budget));
        } return {std::move(value)};}
    default:throw std::invalid_argument("ResidentUnsupportedVariant");
    }
}
// 完整命令回执DTO在主线程转为深拥有值；没有指向常驻组件的引用。
godot::Variant to_variant(const Value& input) {
    return std::visit([](const auto& value)->godot::Variant {
        using T=std::decay_t<decltype(value)>;
        if constexpr(std::is_same_v<T,std::monostate>) return {};
        else if constexpr(std::is_same_v<T,std::string>) return text(value);
        else if constexpr(std::is_same_v<T,Value::Array>) {godot::Array out; for(const auto& item:value) out.push_back(to_variant(item)); return out;}
        else if constexpr(std::is_same_v<T,Value::Dict>) {godot::Dictionary out; for(const auto& item:value) out[text(item.first)]=to_variant(item.second); return out;}
        else return value;
    },input.data);
}
// P2身份只做精确类型/字节形状检查；revision守卫及请求重放先后仍归唯一core。
const Value& live_field(const Value& value,const char* key) {const auto* found=value.find(key);if(!found)throw std::invalid_argument("ResidentInvalidLiveEnvelope");return *found;}
// 字符串不做隐式Variant转换，避免身份字段被重新解释。
std::string live_string(const Value& value) {const auto* found=value.as<StableId>();if(!found)throw std::invalid_argument("ResidentInvalidLiveEnvelope");return *found;}
// int64标签必须真实保留，float/bool不接受为序号或版本。
Integer live_integer(const Value& value) {const auto* found=value.as<Integer>();if(!found)throw std::invalid_argument("ResidentInvalidLiveEnvelope");return *found;}
// 恢复完整或引用身份；业务revision校验不会在这里抢先执行。
WorldIdentity live_identity(const Value& value,bool full) {
    WorldIdentity id;const auto bytes=live_string(live_field(value,"world_id"));if(bytes.size()!=32)throw std::invalid_argument("ResidentInvalidSource");
    const auto digit=[](char c)->int{if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;};
    for(std::size_t i=0;i<id.world_id.size();++i){const auto hi=digit(bytes[2*i]),lo=digit(bytes[2*i+1]);if(hi<0||lo<0)throw std::invalid_argument("ResidentInvalidSource");id.world_id[i]=static_cast<std::uint8_t>(hi*16+lo);}
    id.execution_epoch=live_integer(live_field(value,"execution_epoch"));if(full){id.revision=live_integer(live_field(value,"revision"));id.tick=live_integer(live_field(value,"tick"));}return id;
}
// 只输出有限四字段身份，不读取或导出世界。
godot::Dictionary identity_value(const WorldIdentity& id) {godot::Dictionary value;value["world_id"]=text(world_hex(id.world_id));value["execution_epoch"]=id.execution_epoch;value["revision"]=id.revision;value["tick"]=id.tick;return value;}
// closed kind的字符串只在Godot入口处理，不提供任意方法/查询反射执行。
LiveOperationKind live_kind(const StableId& kind) {
    if(kind=="preview")return LiveOperationKind::preview;if(kind=="command")return LiveOperationKind::command;if(kind=="query")return LiveOperationKind::query;if(kind=="begin_run")return LiveOperationKind::begin_run;if(kind=="grant")return LiveOperationKind::grant;if(kind=="cancel_run")return LiveOperationKind::cancel_run;if(kind=="checkpoint")return LiveOperationKind::checkpoint;if(kind=="restore")return LiveOperationKind::restore;throw std::invalid_argument("ResidentUnknownOperation");
}
// 完成tag保持原请求种类，不拿运行stop替换操作类型。
const char* live_tag(LiveOperationKind kind) {
    switch(kind){case LiveOperationKind::preview:return "preview";case LiveOperationKind::command:return "command";case LiveOperationKind::query:return "query";case LiveOperationKind::begin_run:return "begin_run";case LiveOperationKind::grant:return "grant";case LiveOperationKind::cancel_run:return "cancel_run";case LiveOperationKind::checkpoint:return "checkpoint";case LiveOperationKind::restore:return "restore";}return "invalid";
}
// 仅本进程真实调用用单调时钟，展示等待不计作规则处理。
Integer elapsed(Clock::time_point start) {return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-start).count();}
}
void HanmoResidentBridge::_bind_methods() {
    godot::ClassDB::bind_method(godot::D_METHOD("inspect_checkpoint_selection","checkpoint_bytes"),&HanmoResidentBridge::inspect_checkpoint_selection);
    godot::ClassDB::bind_method(godot::D_METHOD("install","catalog_json","state_json"),&HanmoResidentBridge::install);
    godot::ClassDB::bind_method(godot::D_METHOD("install_native","catalog_json","request_json"),&HanmoResidentBridge::install_native);
    godot::ClassDB::bind_method(godot::D_METHOD("install_checkpoint","catalog_json","checkpoint_bytes","continuation_bytes"),&HanmoResidentBridge::install_checkpoint,godot::Variant());
    godot::ClassDB::bind_method(godot::D_METHOD("enqueue_command","command"),&HanmoResidentBridge::enqueue_command);
    godot::ClassDB::bind_method(godot::D_METHOD("enqueue_command_json","command_json"),&HanmoResidentBridge::enqueue_command_json);
    godot::ClassDB::bind_method(godot::D_METHOD("poll_command","reference"),&HanmoResidentBridge::poll_command);
    godot::ClassDB::bind_method(godot::D_METHOD("debug_export_state"),&HanmoResidentBridge::debug_export_state);
    godot::ClassDB::bind_method(godot::D_METHOD("enqueue_live_operation","operation"),&HanmoResidentBridge::enqueue_live_operation);
    godot::ClassDB::bind_method(godot::D_METHOD("poll_live_operation","reference"),&HanmoResidentBridge::poll_live_operation);
    godot::ClassDB::bind_method(godot::D_METHOD("set_reading_paused","reference","paused"),&HanmoResidentBridge::set_reading_paused);
}
godot::Dictionary HanmoResidentBridge::inspect_checkpoint_selection(const godot::Variant& checkpoint) {
    if(session_)return error("ResidentAlreadyInstalled");
#ifndef HANMO_RESIDENT_CORE_LINKED
    (void)checkpoint;return error("UnsupportedNativeCapability");
#else
    try {
        auto candidate=create_resident_replay_session();if(!candidate)return error("ResidentFactoryFailed");
        auto selected=candidate->inspect_checkpoint_selection(checkpoint_bytes(checkpoint));
        if(!selected.ok)return error(selected.error);
        return to_variant(selected.value);
    }catch(const std::exception& failure){return error(failure.what());}
#endif
}
HanmoResidentBridge::~HanmoResidentBridge() {
    {std::lock_guard<std::mutex> lock(mutex_); stopping_=true; command_.reset(); live_jobs_.clear(); wake_.notify_one();}
    if(worker_.joinable()) worker_.join();
}
godot::Dictionary HanmoResidentBridge::install(const godot::Variant& catalog_json,const godot::Variant& state_json) {
    if(session_) return error("ResidentAlreadyInstalled");
#ifndef HANMO_RESIDENT_CORE_LINKED
    (void)catalog_json; (void)state_json; return error("UnsupportedNativeCapability");
#else
    try {
        const auto start=Clock::now(); auto catalog=parse_boundary_json(initial_bytes(catalog_json)),state=parse_boundary_json(initial_bytes(state_json));
        if(!catalog.ok) return error(catalog.error);
        if(!state.ok) return error(state.error);
        auto candidate=create_resident_replay_session(); if(!candidate) return error("ResidentFactoryFailed");
        const auto installed=candidate->initialize(catalog.value,state.value); if(!installed.ok) return error(installed.error);
        const auto id=candidate->identity(); if(!id.ok) return error(id.error);
        identity_=id.value; live_jobs_.bind(identity_); session_=std::move(candidate);
        try {worker_=std::thread([this]{work();});} catch(...) {session_.reset(); identity_={}; return error("ResidentWorkerStartFailed");}
        godot::Dictionary out; out["ok"]=true; out["world_id"]=text(world_hex(identity_.world_id)); out["epoch"]=identity_.execution_epoch; out["initialization_usec"]=elapsed(start); out["initialization_count"]=1;out["identity"]=identity_value(identity_); return out;
    } catch(const std::exception& failure) {return error(failure.what());}
#endif
}
// 两个冷入口共享原有单worker；只在真实候选完成全部准入后绑定身份和启动线程。
godot::Dictionary HanmoResidentBridge::install_native(const godot::Variant& catalog_json,const godot::Variant& request_json) {
    if(session_)return error("ResidentAlreadyInstalled");
#ifndef HANMO_RESIDENT_CORE_LINKED
    (void)catalog_json;(void)request_json;return error("UnsupportedNativeCapability");
#else
    try {
        const auto start=Clock::now();auto catalog=parse_boundary_json(initial_bytes(catalog_json)),request=parse_boundary_json(initial_bytes(request_json));if(!catalog.ok)return error(catalog.error);if(!request.ok)return error(request.error);
        auto candidate=create_resident_replay_session();if(!candidate)return error("ResidentFactoryFailed");auto installed=candidate->initialize_native(catalog.value,request.value);if(!installed.ok)return error(installed.error);
        auto selected=candidate->installed_selection();if(!selected.ok)return error(selected.error);auto id=candidate->identity();if(!id.ok)return error(id.error);
        godot::Dictionary out=to_variant(selected.value);identity_=id.value;live_jobs_.bind(identity_);session_=std::move(candidate);
        try{worker_=std::thread([this]{work();});}catch(...){session_.reset();identity_={};live_jobs_.clear();return error("ResidentWorkerStartFailed");}
        out["ok"]=true;out["identity"]=identity_value(identity_);out["world_id"]=text(world_hex(identity_.world_id));out["epoch"]=identity_.execution_epoch;out["initialization_count"]=1;out["initialization_usec"]=elapsed(start);return out;
    }catch(const std::exception& failure){return error(failure.what());}
#endif
}
godot::Dictionary HanmoResidentBridge::install_checkpoint(const godot::Variant& catalog_json,const godot::Variant& checkpoint,const godot::Variant& continuation) {
    if(session_)return error("ResidentAlreadyInstalled");
#ifndef HANMO_RESIDENT_CORE_LINKED
    (void)catalog_json;(void)checkpoint;(void)continuation;return error("UnsupportedNativeCapability");
#else
    try {
        const auto start=Clock::now();auto catalog=parse_boundary_json(initial_bytes(catalog_json));if(!catalog.ok)return error(catalog.error);auto bytes=checkpoint_bytes(checkpoint);
        auto candidate=create_resident_replay_session();if(!candidate)return error("ResidentFactoryFailed");Bytes stopped;if(continuation.get_type()!=godot::Variant::NIL)stopped=checkpoint_bytes(continuation,continuation_snapshot_byte_limit);auto installed=candidate->initialize_checkpoint_with_continuation(catalog.value,bytes,stopped);if(!installed.ok)return error(installed.error);
        auto selected=candidate->installed_selection();if(!selected.ok)return error(selected.error);auto id=candidate->identity();if(!id.ok)return error(id.error);
        godot::Dictionary out=to_variant(selected.value);identity_=id.value;live_jobs_.bind(identity_);session_=std::move(candidate);
        try{worker_=std::thread([this]{work();});}catch(...){session_.reset();identity_={};live_jobs_.clear();return error("ResidentWorkerStartFailed");}
        out["ok"]=true;out["identity"]=identity_value(identity_);out["world_id"]=text(world_hex(identity_.world_id));out["epoch"]=identity_.execution_epoch;out["restored"]=true;out["initialization_count"]=0;out["initialization_usec"]=elapsed(start);return out;
    }catch(const std::exception& failure){return error(failure.what());}
#endif
}
godot::Dictionary HanmoResidentBridge::enqueue_command(const godot::Variant& command) {
    const auto start=Clock::now(); if(!session_) return error("ResidentNotInstalled");
    try {
        if(command.get_type()!=godot::Variant::DICTIONARY) return error("ResidentExpectedCommand");
        std::size_t budget=65536; auto converted=from_variant(command,0,budget);
        return enqueue_value(std::move(converted),elapsed(start));
    } catch(const std::exception& failure) {return error(failure.what());}
}
// 诊断JSON入口只解析最多64KiB的小命令；完整state永不通过此热入口。
godot::Dictionary HanmoResidentBridge::enqueue_command_json(const godot::Variant& command_json) {
    const auto start=Clock::now(); if(!session_) return error("ResidentNotInstalled");
    try { const auto bytes=initial_bytes(command_json); if(bytes.size()>65536) return error("ResidentCommandLimit");
        auto parsed=parse_boundary_json(bytes); if(!parsed.ok) return error(parsed.error);
        if(!parsed.value.as<Value::Dict>()) return error("ResidentExpectedCommand");
        return enqueue_value(std::move(parsed.value),elapsed(start));
    } catch(const std::exception& failure) {return error(failure.what());}
}
// 两种传输入口合流为同一小命令值，没有第二条规则派发路径。
godot::Dictionary HanmoResidentBridge::enqueue_value(Value&& converted,Integer input_usec) {
        std::lock_guard<std::mutex> lock(mutex_); if(running_||command_||result_||live_jobs_.busy()||live_jobs_.has_run()) return error("ResidentBridgeBusy");
        if(operation_id_==INT64_MAX) return error("ResidentOperationLimit");
        ++operation_id_; cached_.clear(); live_jobs_.bind(identity_); command_=std::move(converted); input_dto_usec_=input_usec; wake_.notify_one();
        godot::Dictionary out; out["ok"]=true; out["world_id"]=text(world_hex(identity_.world_id)); out["epoch"]=identity_.execution_epoch; out["operation_id"]=operation_id_; return out;
 }
godot::Dictionary HanmoResidentBridge::poll_command(const godot::Variant& reference) {
    if(!session_) return error("ResidentNotInstalled");
    if(reference.get_type()!=godot::Variant::DICTIONARY) return error("ResidentStaleReference");
    godot::Dictionary ref=reference;
    if(!ref.has("world_id")||!ref.has("epoch")||!ref.has("operation_id")||ref["world_id"].get_type()!=godot::Variant::STRING||ref["epoch"].get_type()!=godot::Variant::INT||ref["operation_id"].get_type()!=godot::Variant::INT) return error("ResidentStaleReference");
    std::lock_guard<std::mutex> lock(mutex_);
    if(text(static_cast<godot::String>(ref["world_id"]))!=world_hex(identity_.world_id)||static_cast<Integer>(ref["epoch"])!=identity_.execution_epoch||static_cast<Integer>(ref["operation_id"])!=operation_id_) return error("ResidentStaleReference");
    if(!cached_.is_empty()) return cached_.duplicate(true);
    if(running_||command_) {godot::Dictionary out; out["ok"]=true;out["ready"]=false;return out;}
    if(!result_) return error("ResidentNoCommand");
    const auto start=Clock::now(); auto result=std::move(*result_); result_.reset();
    if(!result.ok) cached_=error(result.error);
    else {cached_["ok"]=true; cached_["preview"]=to_variant(result.value.preview); cached_["receipt"]=to_variant(result.value.receipt); cached_["core_decode_usec"]=static_cast<int64_t>(result.value.timing.decode_usec); cached_["core_preview_usec"]=static_cast<int64_t>(result.value.timing.preview_usec); cached_["core_submit_usec"]=static_cast<int64_t>(result.value.timing.submit_usec); cached_["core_dto_usec"]=static_cast<int64_t>(result.value.timing.dto_usec);}
    cached_["ready"]=true;cached_["current_identity"]=identity_value(identity_); cached_["execute_usec"]=execute_usec_; cached_["input_dto_usec"]=input_dto_usec_; godot::Dictionary output=cached_.duplicate(true); output["output_dto_usec"]=elapsed(start); cached_["output_dto_usec"]=output["output_dto_usec"]; return output;
}
godot::Dictionary HanmoResidentBridge::debug_export_state() {
    if(!session_) return error("ResidentNotInstalled");
    std::lock_guard<std::mutex> lock(mutex_); if(running_||command_||live_jobs_.has_work()) return error("ResidentBridgeBusy");
    const auto state=session_->export_state(); if(!state.ok) return error(state.error); const auto rendered=render_boundary_json(state.value); if(!rendered.ok) return error(rendered.error);
    godot::Dictionary out; out["ok"]=true; out["json"]=text(rendered.value); return out;
}
// source与小payload主线程深复制后即不再读Godot对象；未知/过大输入在接纳前失败。
godot::Dictionary HanmoResidentBridge::enqueue_live_operation(const godot::Variant& input) {
    if(!session_)return error("ResidentNotInstalled");
    try {
        if(input.get_type()!=godot::Variant::DICTIONARY)throw std::invalid_argument("ResidentInvalidLiveEnvelope");
        godot::Dictionary original=input;if(original.size()!=3||!original.has("kind")||original["kind"].get_type()!=godot::Variant::STRING||!original.has("payload")||original["payload"].get_type()!=godot::Variant::DICTIONARY)throw std::invalid_argument("ResidentInvalidLiveEnvelope");
        LiveOperation operation;operation.kind=live_kind(text(static_cast<godot::String>(original["kind"])));godot::Dictionary packet=original.duplicate(false),payload=static_cast<godot::Dictionary>(original["payload"]).duplicate(false);
        const auto extract=[&](const char* key,std::optional<Bytes>& bytes,std::size_t limit=live_checkpoint_byte_limit){if(!payload.has(key))return;if(operation.kind!=LiveOperationKind::checkpoint&&operation.kind!=LiveOperationKind::restore)throw std::invalid_argument("ResidentUnexpectedCheckpointBytes");bytes=checkpoint_bytes(payload[key],limit);payload.erase(key);};
        extract("checkpoint_bytes",operation.checkpoint_bytes);extract("previous_checkpoint_bytes",operation.previous_checkpoint_bytes);extract("continuation_bytes",operation.continuation_bytes,continuation_snapshot_byte_limit);extract("previous_continuation_bytes",operation.previous_continuation_bytes,continuation_snapshot_byte_limit);packet["payload"]=payload;
        std::size_t budget=65536;auto value=from_variant(packet,0,budget);operation.source=live_identity(live_field(value,"source"),true);operation.payload=live_field(value,"payload");
        std::lock_guard<std::mutex> lock(mutex_);if(command_||result_||(running_&&!running_live_))return error("ResidentBridgeBusy");if(operation_id_==INT64_MAX)return error("ResidentOperationLimit");
        const auto source=operation.source;auto accepted=live_jobs_.enqueue(std::move(operation),operation_id_+1);if(!accepted.ok)return error(accepted.error);operation_id_=std::max(operation_id_,accepted.value);wake_.notify_one();
        godot::Dictionary reference;reference["world_id"]=text(world_hex(source.world_id));reference["execution_epoch"]=source.execution_epoch;reference["operation_id"]=accepted.value;godot::Dictionary out;out["ok"]=true;out["reference"]=reference;return out;
    }catch(const std::exception& failure){return error(failure.what());}
}
// 首次ready只消费有限槽位；返回独立Dictionary，调用方不能改写仍被重复读取的缓存。
godot::Dictionary HanmoResidentBridge::poll_live_operation(const godot::Variant& input) {
    if(!session_)return error("ResidentNotInstalled");
    try {
        std::size_t budget=256;auto value=from_variant(input,0,budget);const auto source=live_identity(value,false);const auto id=live_integer(live_field(value,"operation_id"));
        std::lock_guard<std::mutex> lock(mutex_);auto result=live_jobs_.poll(source,id);if(!result.ok)return error(result.error);godot::Dictionary out;out["ok"]=true;out["ready"]=result.value.ready;if(!result.value.ready)return out;
        out["tag"]=live_tag(result.value.tag);out["source_identity"]=identity_value(result.value.source);out["current_identity"]=identity_value(result.value.current);godot::Variant owned=to_variant(result.value.result);if(result.value.checkpoint_bytes||result.value.continuation_bytes){if(owned.get_type()!=godot::Variant::DICTIONARY)throw std::runtime_error("ResidentInvalidColdResult");godot::Dictionary dictionary=owned;if(result.value.checkpoint_bytes)dictionary["checkpoint_bytes"]=packed_checkpoint(*result.value.checkpoint_bytes);if(result.value.continuation_bytes)dictionary["continuation_bytes"]=packed_checkpoint(*result.value.continuation_bytes);owned=dictionary;}out["result"]=owned;return out;
    }catch(const std::exception& failure){return error(failure.what());}
}
// reading门只有撤权，不调用领域规则、不等待worker、不把解除暂停解释成推进。
godot::Dictionary HanmoResidentBridge::set_reading_paused(const godot::Variant& input,bool paused) {
    if(!session_)return error("ResidentNotInstalled");
    try {
        std::size_t budget=65536;auto value=from_variant(input,0,budget);const auto source=live_identity(value,false);RunRef ref{source.world_id,source.execution_epoch,live_string(live_field(value,"run_id"))};std::lock_guard<std::mutex> lock(mutex_);auto result=live_jobs_.set_reading_paused(ref,paused);if(!result.ok)return error(result.error);godot::Dictionary out;out["ok"]=true;out["reading_paused"]=paused;return out;
    }catch(const std::exception& failure){return error(failure.what());}
}
// 唯一worker在每个完整调用后发布结果；普通任务/取消调度都不创建第二份world。
void HanmoResidentBridge::work() {
    while(true) {
        Value command;std::optional<ResidentLiveTicket> live;WorldIdentity current;
        {std::unique_lock<std::mutex> lock(mutex_);wake_.wait(lock,[this]{return stopping_||command_.has_value()||live_jobs_.has_work();});if(stopping_)return;
            if(command_){command=std::move(*command_);command_.reset();running_live_=false;}else{live=live_jobs_.take();if(!live)continue;running_live_=true;}running_=true;current=identity_;}
        const auto start=Clock::now();Outcome<ReplayCommandResult> replay;Outcome<LiveOperationResult> actual;
        try {if(live)actual=session_->execute_live(live->operation);else replay=session_->execute(command);}catch(...){RuleError failure{{"ResidentBridgeInternalError","resident_bridge"},ErrorKind::internal,{},{},{},{}};if(live)actual.error=failure;else replay.error=failure;}
        try {auto observed=session_->identity();if(observed.ok)current=observed.value;}catch(...){}
        {std::lock_guard<std::mutex> lock(mutex_);identity_=current;if(live)live_jobs_.complete(live->operation_id,std::move(actual),current);else{execute_usec_=elapsed(start);result_=std::move(replay);live_jobs_.bind(current);}running_=false;running_live_=false;}
    }
}
