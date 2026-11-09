// C域冷边界的标量和原序容器工具；具体字段schema仍逐个由军事/交互owner定义。
#pragma once
#include "resident/storage/common_codec.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
namespace hanmo::resident::domain_codec {
using V=BoundaryValue;using D=V::Dict;using A=V::Array;
inline V enc(const StableId& v){return V{v};}
inline V enc(Integer v){return V{v};}
inline V enc(bool v){return V{v};}
inline void dec(const V& v,StableId& out,const StableId& p){out=boundary::string(v,p);}
inline void dec(const V& v,Integer& out,const StableId& p){out=boundary::integer(v,p);}
inline void dec(const V& v,bool& out,const StableId& p){out=boundary::boolean(v,p);}
inline Status visit(CodecVisitor& c,StableId& v){return c.field("value",v);}
inline Status visit(CodecVisitor& c,Integer& v){return c.field("value",v);}
inline Status visit(CodecVisitor& c,bool& v){return c.field("value",v);}
template<class T> V enc_list(const std::vector<T>& rows){A a;for(auto& r:rows)a.push_back(enc(r));return V{std::move(a)};}
// These are original collector allocation boundaries, not field-schema walks.
// The outer decoded candidate owns every ordinary reservation through adoption.
template<class T>void prepare_decoded_collection(std::size_t count,std::string_view path){
    if(count>std::vector<T>{}.max_size())boundary::fail("CheckpointCollectionTooLarge",path);
    if(auto* budget=ScopedCheckpointJsonDecode::current()){
        const auto slash=path.find_last_of('/');
        const auto field=slash==std::string_view::npos?path:path.substr(slash+1);
        budget->collection(field,count);
    }
    checkpoint_decode_boundary_array(count,sizeof(T));
    if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<T>(count);
    else if(auto* budget=ScopedCheckpointBoundaryDecode::current())budget->default_values<T>(count);
}
template<class T,class DecodeRow>void dec_list(const V& v,std::vector<T>& rows,const StableId& p,DecodeRow decode_row){
    const auto& input=boundary::array(v,p);
    CheckpointJsonVisitDepth depth(ScopedCheckpointJsonDecode::current()!=nullptr);
    prepare_decoded_collection<T>(input.size(),p);
    // Count construction owns the final exact shell. No push/growth, temporary
    // T, move_if_noexcept, or uncharged copy is hidden in element transfer.
    std::vector<T> decoded(input.size());
    for(std::size_t i=0;i<input.size();++i){
        char digits[3*sizeof(std::size_t)+1];auto value=i;auto* end=digits+sizeof(digits);auto* first=end;
        do{*--first=char('0'+value%10);value/=10;}while(value);
        const auto path=checkpoint_decode_boundary_join({p,"/",std::string_view(first,static_cast<std::size_t>(end-first))});
        decode_row(input[i],decoded[i],path);
    }
    rows.swap(decoded);
}
template<class T>void dec_list(const V& v,std::vector<T>& rows,const StableId& p){
    dec_list(v,rows,p,[](const V& input,T& output,const StableId& path){dec(input,output,path);});
}
template<class T> V enc_pairs(const std::vector<std::pair<StableId,T>>& rows){D d;for(auto& r:rows)d.emplace_back(r.first,enc(r.second));return V{std::move(d)};}
template<class T,class DecodeRow>void dec_pairs(const V& v,std::vector<std::pair<StableId,T>>& rows,const StableId& p,DecodeRow decode_row){
    const auto& input=boundary::object(v,p);
    CheckpointJsonVisitDepth depth(ScopedCheckpointJsonDecode::current()!=nullptr);
    using Row=std::pair<StableId,T>;prepare_decoded_collection<Row>(input.size(),p);
    std::vector<Row> decoded(input.size());
    for(std::size_t i=0;i<input.size();++i){
        const auto& source=input[i];auto& destination=decoded[i];
        const auto path=checkpoint_decode_boundary_join({p,"/",source.first});
        decode_row(source.second,destination.second,path);
        auto key=checkpoint_decode_boundary_string(source.first);destination.first.swap(key);
    }
    // Duplicate handling and insertion order remain each original schema's
    // responsibility; no sorting, filtering or new semantic check is added.
    rows.swap(decoded);
}
template<class T>void dec_pairs(const V& v,std::vector<std::pair<StableId,T>>& rows,const StableId& p){
    dec_pairs(v,rows,p,[](const V& input,T& output,const StableId& path){dec(input,output,path);});
}
// 原kind错误转换为能力边界，保留完整来源字段给事务错误回执。
inline RuleError unsupported(const StableId& domain,const StableId& kind,const StableId& source){return {{"UnsupportedNativeCapability",{}},ErrorKind::unsupported_capability,"decode",domain,kind,source};}
}
namespace hanmo::resident::domain_codec {
// 元数据附在业务记录本身；只记字典键序，不重复存任何字段值或数组路径。
inline std::vector<StableId> order(const V& v){return boundary::order(v);}
inline V ordered(V v,const std::vector<StableId>& keys){auto r=apply_field_order(v,keys);if(!r.ok)boundary::fail(r.error.original.code,r.error.original.path);return v;}
inline Status visit_order(CodecVisitor& c,std::vector<StableId>& keys){return c.sequence("field_order",keys,[](auto& codec,auto& key){return codec.field("key",key);});}
inline bool had(const std::vector<StableId>& keys,const StableId& key){return std::find(keys.begin(),keys.end(),key)!=keys.end();}
}
