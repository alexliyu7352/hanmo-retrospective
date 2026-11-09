// 新具名工作/设施结构共用严格字段遍历；原Time2旧出口保持原字节，不创建通用存档黑箱。
#pragma once
#include "world_timeline.hpp"
#include "world_timeline_codec.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include <optional>
#include <string_view>
#include <limits>
#include <stdexcept>
#include <type_traits>
namespace hanmo::resident::exact_fields {
struct CodecFailure { RuleError error; };

template<class T> T take(Outcome<T> value){if(!value.ok)throw CodecFailure{std::move(value.error)};return std::move(value.value);}
template<class T,class Fn> Outcome<T> guarded(Fn fn){try{return {true,fn(),{}};}catch(CodecFailure& e){return {false,{},std::move(e.error)};}catch(const std::bad_alloc&){RuleError e;e.original={"ExactCodecResourceExhausted","exact_structure_codec"};e.kind=ErrorKind::internal;return {false,{},std::move(e)};}catch(const std::length_error&){RuleError e;e.original={"ExactCodecResourceExhausted","exact_structure_codec"};e.kind=ErrorKind::internal;return {false,{},std::move(e)};}}
struct JsonBudget { std::size_t bytes=0,nodes=0; };
// The original exact schema policy borrows the sole JSON archive. Its cursor
// checks field order and its old JsonBudget limits wire complexity only; neither
// is an allocation ledger. Actual text/default/vector/optional construction is
// performed once by ActorFoundationJsonVisitor and its same-job decode owner.
class JsonCodec {
public:
    JsonCodec(const BoundaryValue& input,JsonBudget& budget,std::size_t depth=0,bool scalar=false)
      :owned_(std::in_place,input,scalar),archive_(&*owned_),input_(&input),budget_(budget),depth_(depth),scalar_(scalar){inspect();}
    JsonCodec(BoundaryValue& output,JsonBudget& budget,std::size_t depth=0,bool scalar=false)
      :owned_(std::in_place,output,scalar),archive_(&*owned_),budget_(budget),depth_(depth),scalar_(scalar){inspect();}
    bool reading()const{return archive_->reading();}
    Status status()const{return result_;}
    Status reject(std::string_view code,std::string_view path="exact_structure_codec"){if(result_.ok)result_=archive_->reject(code,path);return result_;}
    Status absorb(Status s){if(result_.ok&&!s.ok)result_=std::move(s);return result_;}
    Status finish(){
        if(result_.ok&&input_&&!scalar_&&position_!=input_->as<BoundaryValue::Dict>()->size())reject("UnknownExactStructureField");
        if(result_.ok)absorb(archive_->finish());return result_;
    }
    template<class T>Status prepare_default_values(std::uint64_t count=1){if(result_.ok)absorb(archive_->template prepare_default_values<T>(count));return result_;}
    Status field(std::string_view name,StableId& value){return primitive(name,value);}
    Status field(std::string_view name,Integer& value){return primitive(name,value);}
    Status unsigned_field(std::string_view name,std::uint64_t& value){
        if(!result_.ok)return result_;
        if(reading()){
            const auto* node=read_node(name);if(!node)return result_;
            if(!node->as<std::uint64_t>()){const auto* n=node->as<Integer>();if(!n||*n<0)return reject("ExactStructureUnsignedIntegerRequired",name);}
            if(!charge(sizeof(value)))return result_;
        }else if(!charge(sizeof(value))||!write_field(name))return result_;
        absorb(archive_->unsigned_field(name,value));return result_;
    }
    template<class Fn>Status object(std::string_view name,Fn fn){
        if(!result_.ok)return result_;const auto* node=reading()?read_node(name):nullptr;
        if(!result_.ok||(!reading()&&!write_field(name)))return result_;
        absorb(archive_->object(name,[&](auto& original){JsonCodec child(original,node,budget_,depth_+1,false);if(child.status().ok)child.absorb(fn(child));return child.finish();}));return result_;
    }
    template<class T,class Allocator,class Fn>Status sequence(std::string_view name,std::vector<T,Allocator>& values,Fn fn){
        if(!result_.ok)return result_;constexpr bool scalar=is_scalar<T>();
        const BoundaryValue::Array* rows=nullptr;
        if(reading()){const auto* node=read_node(name);if(!node)return result_;rows=node->as<BoundaryValue::Array>();if(!rows)return reject("ExactStructureArrayRequired",name);if(rows->size()>CodecVisitor::max_items)return reject("CheckpointCollectionTooLarge",name);}
        else{if(values.size()>CodecVisitor::max_items)return reject("CheckpointCollectionTooLarge",name);if(!write_field(name))return result_;}
        std::size_t index=0;
        absorb(archive_->sequence(name,values,[&](auto& original,auto& value){const auto* node=rows?&(*rows)[index]:nullptr;++index;JsonCodec child(original,node,budget_,depth_+1,scalar);if(child.status().ok)child.absorb(fn(child,value));return child.finish();},scalar));return result_;
    }
    template<class T,class Fn>Status optional(std::string_view name,std::optional<T>& value,Fn fn){
        if(!result_.ok)return result_;constexpr bool scalar=is_scalar<T>();const auto* node=reading()?read_node(name):nullptr;
        if(!result_.ok||(!reading()&&!write_field(name)))return result_;
        absorb(archive_->optional(name,value,[&](auto& original,auto& target){JsonCodec child(original,node,budget_,depth_+1,scalar);if(child.status().ok)child.absorb(fn(child,target));return child.finish();},scalar));return result_;
    }
private:
    std::optional<ActorFoundationJsonVisitor> owned_;ActorFoundationJsonVisitor* archive_;
    const BoundaryValue* input_=nullptr;JsonBudget& budget_;std::size_t depth_,position_=0;bool scalar_;Status result_{true,{},{}};
    JsonCodec(ActorFoundationJsonVisitor& archive,const BoundaryValue* input,JsonBudget& budget,std::size_t depth,bool scalar)
      :archive_(&archive),input_(input),budget_(budget),depth_(depth),scalar_(scalar){inspect();}
    template<class T>static constexpr bool is_scalar(){return std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,std::uint64_t>||std::is_enum_v<T>;}
    bool charge(std::size_t bytes){if(!result_.ok)return false;if(++budget_.nodes>2000000u||bytes>CodecVisitor::max_bytes-budget_.bytes){reject("CheckpointTooLarge");return false;}budget_.bytes+=bytes;return true;}
    void inspect(){
        if(depth_>=CodecVisitor::max_depth){reject("CheckpointTooDeep");return;}if(!charge(1))return;
        if(input_&&!scalar_){const auto* rows=input_->as<BoundaryValue::Dict>();if(!rows){reject("ExactStructureObjectRequired");return;}if(rows->size()>CodecVisitor::max_items){reject("CheckpointCollectionTooLarge");return;}
            for(std::size_t i=0;i<rows->size();++i){if(!charge((*rows)[i].first.size()))return;for(std::size_t prior=0;prior<i;++prior)if((*rows)[prior].first==(*rows)[i].first){reject("DuplicateExactStructureField",(*rows)[i].first);return;}}
        }
    }
    const BoundaryValue* read_node(std::string_view name){
        if(!result_.ok)return nullptr;if(scalar_)return input_;const auto& rows=*input_->as<BoundaryValue::Dict>();
        if(position_>=rows.size()||rows[position_].first!=name){reject("ExactStructureFieldOrderMismatch",name);return nullptr;}
        return &rows[position_++].second;
    }
    // The original archive owns duplicate-visit checking and output insertion.
    // This side only retains the exact writer's existing key complexity charge.
    bool write_field(std::string_view name){if(scalar_)return true;if(!charge(name.size()))return false;if(archive_->has_written_field(name)){reject("DuplicateExactStructureField",name);return false;}return true;}
    template<class T>Status primitive(std::string_view name,T& value){
        if(!result_.ok)return result_;
        if(reading()){const auto* node=read_node(name);if(!node)return result_;const auto* typed=node->as<T>();if(!typed)return reject("ExactStructureScalarTypeMismatch",name);if constexpr(std::is_same_v<T,StableId>){if(!charge(typed->size()))return result_;}else if(!charge(sizeof(T)))return result_;}
        else{if constexpr(std::is_same_v<T,StableId>){if(!charge(value.size()))return result_;}else if(!charge(sizeof(T)))return result_;if(!write_field(name))return result_;}
        absorb(archive_->field(name,value));return result_;
    }
};
// 所有有限tag固定整数序；非法内存枚举同损坏输入一样拒绝。
template<class C> Status finite(C& c,const char* key,Integer& tag,Integer maximum){c.field(key,tag);if(c.status().ok&&(tag<0||tag>maximum))return c.reject("UnknownExactStructureTag",key);return c.status();}
template<class C> Status required_ref(C& c,const char* key,StableId& value){c.field(key,value);if(c.status().ok&&value.empty())return c.reject("MissingExactStructureSource",key);return c.status();}
template<class C> Status optional_ref(C& c,const char* key,std::optional<StableId>& value){return c.optional(key,value,[](auto& x,auto& v){return required_ref(x,"value",v);});}
template<class C> Status references(C& c,const char* key,std::vector<StableId>& values){std::size_t position=0;return c.sequence(key,values,[&](auto& x,auto& v){required_ref(x,"value",v);if(x.status().ok)for(std::size_t i=0;i<position;++i)if(values[i]==v)return x.reject("DuplicateExactStructureReference",key);++position;return x.status();});}
// SourceRef保持三字段，业务schema由每一使用位置独立持有和核正。
template<class C> Status source(C& c,SourceRef& s){required_ref(c,"domain",s.domain);required_ref(c,"kind",s.kind);return required_ref(c,"id",s.id);}
template<class C> Status schema(C& c,const char* key,Integer& value){c.field(key,value);if(c.status().ok&&(value<=0||static_cast<std::uint64_t>(value)>std::numeric_limits<std::uint32_t>::max()))return c.reject("InvalidExactStructureSourceSchema",key);return c.status();}
// fraction冷读必须已约分，零仅0/1；不在codec中将损坏档静默规范化。
template<class C> Status fraction(C& c,CanonicalFraction& value){if constexpr(std::is_same_v<std::decay_t<C>,CodecVisitor>)(void)0;EncodedFraction encoded;if(!c.reading())encoded=take(encode_canonical_fraction(value));c.field("numerator",encoded.numerator);c.field("denominator",encoded.denominator);if(c.status().ok&&c.reading())value=take(decode_canonical_fraction(encoded));return c.status();}
// 工量保RWM声明的value强型层，不能与裸fraction压平成同号不同布局。
template<class C> Status work_units(C& c,WorkUnits& value){return c.object("value",[&](auto& x){return fraction(x,value.value);});}
// Moment字段序与冻结叶一致，余量>=1直接失败，不进位改写保存时点。
template<class C> Status moment(C& c,WorldMoment& value){c.field("half_day_tick",value.half_day_tick);c.object("sub_half_day",[&](auto& x){return fraction(x,value.sub_half_day);});if(c.status().ok)take(encode_world_moment(value));return c.status();}
// 目录锁沿原CheckpointIdentity完整顺序，module_versions保原顺序且拒重复名。
template<class C> Status catalog(C& c,CatalogIdentity& v){required_ref(c,"content_id",v.content_id);required_ref(c,"content_version",v.content_version);required_ref(c,"artifact_sha256",v.artifact_sha256);required_ref(c,"content_digest",v.content_digest);required_ref(c,"ruleset_id",v.ruleset_id);required_ref(c,"schema_sha256",v.schema_sha256);optional_ref(c,"pack_set_digest",v.pack_set_digest);std::size_t position=0;c.sequence("module_versions",v.module_versions,[&](auto& x,auto& p){required_ref(x,"name",p.first);required_ref(x,"version",p.second);if(x.status().ok)for(std::size_t i=0;i<position;++i)if(v.module_versions[i].first==p.first)return x.reject("DuplicateExactStructureModule");++position;return x.status();});return c.status();}

// 字段级相同visitor供JSON和binary；输出只拥有本次深值，领域source验证另由实际reader负责。
template<class T,class Fn> Outcome<T> decode_json(const BoundaryValue& input,Fn fn){return guarded<T>([&]{JsonBudget budget;JsonCodec codec(input,budget);take(codec.template prepare_default_values<T>());T candidate;take(fn(codec,candidate));take(codec.finish());return candidate;});}
template<class T,class Fn> Outcome<BoundaryValue> encode_json(const T& input,Fn fn){return guarded<BoundaryValue>([&]{auto copied_fields=CheckpointMemoryCharge(0);T candidate=input;BoundaryValue out;JsonBudget budget;JsonCodec codec(out,budget);take(fn(codec,candidate));take(codec.finish());return out;});}
template<class T,class Fn> Status visit_binary(CodecVisitor& codec,T& value,Fn fn){return guarded<Done>([&]{if(codec.reading()){take(codec.template prepare_default_values<T>());T candidate;take(fn(codec,candidate));value=std::move(candidate);}else take(fn(codec,value));return Done{};});}
}
