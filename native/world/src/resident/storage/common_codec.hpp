// 仅冷装载、审计导出边界使用有序JSON；热候选内只存各域typed字段。
#pragma once
#include "common_store.hpp"
#include "../checkpoint/ordered_value.hpp"
#include <stdexcept>
#include <string_view>
#include "../checkpoint/checkpoint_json_decode.hpp"
namespace hanmo::resident {
// 仅中央已锁Common组合传入；不可依据某个lot当前是否有引用选择布局。
struct CommonResourceCodecContext { bool item_instance_refs_enabled=false,lot_property_refs_enabled=false,domain_activity_reservation_refs_enabled=false,carrier_location_enabled=false; };
namespace boundary {
// 解码失败按原字段报告；异常只在codec内部使用，公共结果不抛出格式错误。
struct DecodeFailure : std::runtime_error { RuleError error;DecodeFailure(std::string code,std::string path):std::runtime_error(code),error{{std::move(code),std::move(path)},ErrorKind::rule,{},{},{},{}}{} explicit DecodeFailure(RuleError failure):std::runtime_error(failure.original.code),error(std::move(failure)){} };
[[noreturn]] inline void fail_parts(std::string_view code,std::initializer_list<std::string_view> parts,ErrorKind kind=ErrorKind::rule) {
    if(checkpoint_boundary_decode_active())checkpoint_decode_boundary_failure(code,parts,kind);
    std::string path;for(auto part:parts)if(!part.empty())path.append(part.data(),part.size());
    DecodeFailure failure(std::string(code),std::move(path));failure.error.kind=kind;throw failure;
}
[[noreturn]] inline void fail(std::string_view code,std::string_view path) {fail_parts(code,{path});}
[[noreturn]] inline void unsupported(std::string_view code,std::string_view path) {fail_parts(code,{path},ErrorKind::unsupported_capability);}
// Reuse the original first-match lookup; a borrowed key never allocates.
[[gnu::no_dangling]] inline const BoundaryValue& field(const BoundaryValue& o,std::string_view key,std::string_view path) {if(const auto* value=o.find(key))return *value;fail_parts("MissingResidentField",{path,"/",key});}
inline Integer integer(const BoundaryValue& v,std::string_view path) {auto* p=std::get_if<Integer>(&v.data);if(!p)fail("InvalidResidentInteger",path);return *p;}
// Only a declared uint64 field accepts the unsigned alternative.
inline std::uint64_t unsigned_integer(const BoundaryValue& v,std::string_view path) {
    if(const auto* p=v.as<std::uint64_t>())return *p;
    if(const auto* p=v.as<Integer>();p&&*p>=0)return static_cast<std::uint64_t>(*p);
    fail("InvalidResidentUnsignedInteger",path);
}
inline bool boolean(const BoundaryValue& v,std::string_view path) {auto* p=std::get_if<bool>(&v.data);if(!p)fail("InvalidResidentBoolean",path);return *p;}
// One original string type gate for borrowed inspection and owning decoding.
// The reference only lives as long as the caller's unchanged boundary value.
[[gnu::no_dangling]] inline const StableId& string_ref(const BoundaryValue& v,std::initializer_list<std::string_view> path) {const auto* text=v.as<std::string>();if(!text)fail_parts("InvalidResidentString",path);return *text;}
inline StableId string(const BoundaryValue& v,std::string_view path) {return checkpoint_decode_boundary_string(string_ref(v,{path}));}
inline double real(const BoundaryValue& v,std::string_view path) {auto* p=std::get_if<double>(&v.data);if(!p)fail("InvalidResidentDouble",path);return *p;}
[[gnu::no_dangling]] inline const BoundaryValue::Dict& object(const BoundaryValue& v,std::string_view path) {auto* p=std::get_if<BoundaryValue::Dict>(&v.data);if(!p)fail("InvalidResidentObject",path);return *p;}
[[gnu::no_dangling]] inline const BoundaryValue::Array& array(const BoundaryValue& v,std::string_view path) {auto* p=std::get_if<BoundaryValue::Array>(&v.data);if(!p)fail("InvalidResidentArray",path);return *p;}
inline void fields(const BoundaryValue& v,std::initializer_list<const char*> required,std::initializer_list<const char*> optional,std::string_view path) {auto& rows=object(v,path);for(auto k:required)field(v,k,path);for(auto& row:rows)if(std::none_of(required.begin(),required.end(),[&](auto k){return row.first==k;})&&std::none_of(optional.begin(),optional.end(),[&](auto k){return row.first==k;}))fail_parts("UnsupportedResidentField",{path,"/",row.first});}
// Original per-index string validation, shared by the owning vector decoder
// and callers that only validate the already owned immutable array.
template<class Fn>void visit_string_rows(const BoundaryValue::Array& rows,std::string_view path,Fn&& visit){
    for(std::size_t i=0;i<rows.size();++i){char digits[3*sizeof(std::size_t)+1];auto value=i;auto* end=digits+sizeof(digits);auto* start=end;do{*--start=char('0'+value%10);value/=10;}while(value);visit(string_ref(rows[i],{path,"/",std::string_view(start,static_cast<std::size_t>(end-start))}));}
}
inline void validate_strings(const BoundaryValue& v,std::string_view path){visit_string_rows(array(v,path),path,[](const StableId&){});}
inline std::vector<StableId> strings(const BoundaryValue& v,std::string_view path) {
    const auto& rows=array(v,path);std::vector<StableId> result;
    checkpoint_decode_boundary_array(rows.size(),sizeof(StableId));result.reserve(rows.size());
    visit_string_rows(rows,path,[&](const StableId& text){result.push_back(checkpoint_decode_boundary_string(text));});
    return result;
}
inline Integer get_int(const BoundaryValue& o,std::string_view k,std::string_view p) {const auto& value=field(o,k,p);const auto* result=value.as<Integer>();if(!result)fail_parts("InvalidResidentInteger",{p,"/",k});return *result;}
[[gnu::no_dangling]] inline const StableId& get_string_ref(const BoundaryValue& o,std::string_view k,std::string_view p) {return string_ref(field(o,k,p),{p,"/",k});}
inline StableId get_string(const BoundaryValue& o,std::string_view k,std::string_view p) {return checkpoint_decode_boundary_string(get_string_ref(o,k,p));}
inline bool get_bool(const BoundaryValue& o,std::string_view k,std::string_view p) {const auto& value=field(o,k,p);const auto* result=value.as<bool>();if(!result)fail_parts("InvalidResidentBoolean",{p,"/",k});return *result;}
inline BoundaryValue obj(std::initializer_list<std::pair<std::string,BoundaryValue>> v) {return BoundaryValue{BoundaryValue::Dict(v)};}
inline BoundaryValue string_array(const std::vector<StableId>& v) {BoundaryValue::Array a;for(auto& s:v)a.push_back(BoundaryValue{s});return BoundaryValue{std::move(a)};}
inline std::vector<StableId> order(const BoundaryValue& v){
    const auto* rows=v.as<BoundaryValue::Dict>();if(!rows)fail("ExpectedBoundaryDictionary","boundary");
    std::vector<StableId> keys;checkpoint_decode_boundary_array(rows->size(),sizeof(StableId));keys.reserve(rows->size());
    // Original first duplicate and insertion order, without an uncharged set.
    for(const auto& row:*rows){for(const auto& prior:keys)if(prior==row.first)fail("DuplicateBoundaryKey","boundary");keys.push_back(checkpoint_decode_boundary_string(row.first));}
    return keys;
}
inline BoundaryValue ordered(BoundaryValue v,const std::vector<StableId>& keys){auto r=apply_field_order(v,keys);if(!r.ok)fail(r.error.original.code,r.error.original.path);return v;}
ReservationRef decode_reservation_ref(const BoundaryValue&,std::string_view,bool domain_activity_enabled=false);
BoundaryValue encode_reservation_ref(const ReservationRef&,bool typed=false,bool domain_activity_enabled=false);
LotView decode_lot(const BoundaryValue&,std::string_view,const CommonResourceCodecContext&);
WalletView decode_wallet_frame(const BoundaryValue&,std::string_view,bool typed,const CommonResourceCodecContext&);
InventoryView decode_inventory_frame(const BoundaryValue&,std::string_view,const CommonResourceCodecContext&);
BoundaryValue encode_lot(const LotView&,bool typed,const CommonResourceCodecContext&);
Owner decode_owner(const BoundaryValue&,std::string_view);Location decode_location(const BoundaryValue&,std::string_view,bool carrier_location_enabled=false);
BoundaryValue encode_owner(const Owner&);BoundaryValue encode_location(const Location&,bool carrier_location_enabled=false);
} // 领域owner独立逐字段解码，禁止将未实现动态领域保存在BoundaryValue extras。
class CommonResultCodec;
class ProjectionWriter;
Outcome<CommonCatalog> decode_common_catalog(const BoundaryValue& data);
// Same original Common row schema and field order, borrowed by immutable
// domain result evidence. The caller supplies the actual selected resource
// layout and the already-active original JSON decode context.
Outcome<WalletView> decode_common_wallet_frame(const BoundaryValue&,std::string_view path,bool typed,const CommonResourceCodecContext&,ScopedCheckpointJsonDecode&);
Outcome<InventoryView> decode_common_inventory_frame(const BoundaryValue&,std::string_view path,const CommonResourceCodecContext&,ScopedCheckpointJsonDecode&);
Status emit_common_wallet_frame_fields(ProjectionWriter&,const WalletView&,bool typed,const CommonResourceCodecContext&);
Status emit_common_inventory_frame_fields(ProjectionWriter&,const InventoryView&,bool typed,const CommonResourceCodecContext&);
Status emit_common_wallet_frame(ProjectionWriter&,std::string_view name,const WalletView&,bool typed,const CommonResourceCodecContext&);
Status emit_common_inventory_frame(ProjectionWriter&,std::string_view name,const InventoryView&,bool typed,const CommonResourceCodecContext&);

// 组合装载只解码结构；完整Common校验必须等WorldBundle绑定任务/家庭/军队真实读口后执行。
enum class CommonSeedValidation { standalone, defer_until_bundle };
Outcome<CommonState> decode_common_seed(const BoundaryValue& state,const CommonCatalog&,const CommonResultCodec* results=nullptr,CommonSeedValidation validation=CommonSeedValidation::standalone);
// Borrowed from the same WorldBundle for a single projection. The complete
// world supplies the real owner dispatch; null retains the original Common-only
// export. A fact is emitted as a whole row because its legacy kind may differ
// from its common reference header. No payload tree or second owner is retained.
class CommonProjectionPayloads {
public:
    virtual ~CommonProjectionPayloads()=default;
    virtual Status emit_action_fields(ProjectionWriter&,const ActionCoreView&)const=0;
    virtual Status emit_fact_record(ProjectionWriter&,const StableId& name,const FactRecord&)const=0;
};
// 与旧JSON导出同路径；fields仅供世界原字段合成器在自己的根内调用。
Status emit_common_state_fields(ProjectionWriter&,const CommonState&,const CommonResultCodec* results=nullptr,const CommonResourceCodecContext& resources={},const CommonProjectionPayloads* payloads=nullptr);
Status emit_common_state(ProjectionWriter&,const StableId& name,const CommonState&,const CommonResultCodec* results=nullptr,const CommonResourceCodecContext& resources={},const CommonProjectionPayloads* payloads=nullptr);
BoundaryValue encode_common_state(const CommonState&,const CommonResultCodec* results=nullptr,const CommonResourceCodecContext& resources={},const CommonProjectionPayloads* payloads=nullptr);
} // action.params/fact payload/result.details的codec由唯一owner在外层合成。
