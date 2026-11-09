// 原typed JSON record callback的只写适配；策略由原writer类型显式选择，绝不按payload猜形状。
#pragma once
#include "projection_writer.hpp"
#include "resident/model/common_model.hpp"
namespace hanmo::resident {
enum class ProjectionRecordShape { scalar_null_dictionary, object_presence_sequence, exact_scalar_null_dictionary };
template<ProjectionRecordShape Shape>class ProjectionRecordArchive {
public:
    explicit ProjectionRecordArchive(ProjectionWriter& writer):writer_(writer){}
    bool reading()const noexcept{return false;}
    // This original archive only writes borrowed values. Shared finite field
    // callbacks may ask before a read-side construction; none occurs here.
    template<class T>Status prepare_default_values(std::uint64_t=1){return status();}
    Status status()const{return foreign_result_.ok?writer_.status():foreign_result_;}
    Status finish()const{return status();}
    Status reject(std::string_view code,std::string_view path={}){if(!status().ok)return status();return writer_.reject(code,path);}
    template<class T>Status field(std::string_view name,T& value){if(!begin_fields())return status();return writer_.field(scalar_?scalar_name_:std::string_view(name),value);}
    Status unsigned_field(std::string_view name,std::uint64_t& value){if(!begin_fields())return status();return writer_.unsigned_field(scalar_?scalar_name_:std::string_view(name),value);}
    template<class F>Status object(std::string_view name,F visit){if(!begin_fields())return status();return writer_.object(name,[&](auto& child){ProjectionRecordArchive archive(child);return visit(archive);});}
    template<class T,class F>Status optional(std::string_view name,std::optional<T>& value,F visit){
        if(!begin_fields())return status();
        if constexpr(Shape==ProjectionRecordShape::object_presence_sequence){return object(name,[&](auto& child){bool present=value.has_value();child.field("present",present);if(present&&child.status().ok)return visit(child,*value);return child.status();});}
        else {if(!value)return writer_.null(name);return item(name,*value,visit);}
    }
    template<class T,class Allocator,class F>Status sequence(std::string_view name,std::vector<T,Allocator>& values,F visit){
        if(!begin_fields())return status();
        return writer_.sequence(name,values,[&](auto&,const T& value){return item("",const_cast<T&>(value),visit);});
    }
    template<class Range,class F>Status write_sequence(std::string_view name,const Range& values,F visit){
        if(!begin_fields())return status();
        return writer_.sequence(name,values,[&](auto&,const auto& value){using T=std::remove_const_t<std::remove_reference_t<decltype(value)>>;return item("",const_cast<T&>(value),visit);});
    }
    template<class T,class F>Status table(std::string_view name,OrderedTable<T>& rows,F visit){
        if(!begin_fields())return status();
        static_assert(Shape!=ProjectionRecordShape::object_presence_sequence,"dynamic table must use its original sequence helper");
        return writer_.object(name,[&](auto& child){for(const auto& row:rows.rows()){child.object(row.first,[&](auto& record){ProjectionRecordArchive archive(record);return visit(archive,const_cast<T&>(row.second));});if(!child.status().ok)break;}return child.status();});
    }
    // 仅整个当前object转交原同字段出口；调用方不能前后拼入另一套字段。
    // decode参数只保证与JSON reader同一typed callback签名，写端绝不解码或构值树。
    template<class T,class Decode,class EmitFields>Status foreign_record(T& value,Decode&& decode,EmitFields&& emit_fields){
        (void)decode;if(!status().ok)return status();
        if(fields_started_||foreign_||scalar_)return reject("ForeignRecordRequiresExclusiveObject");
        foreign_=true;
        auto emitted=emit_fields(writer_,static_cast<const T&>(value));
        if(!emitted.ok)foreign_result_=std::move(emitted);
        return status();
    }
private:
    ProjectionWriter& writer_;bool scalar_=false,fields_started_=false,foreign_=false;std::string_view scalar_name_;Status foreign_result_=common_success();
    bool begin_fields(){if(!status().ok)return false;if(foreign_){reject("ForeignRecordAlreadyConsumed");return false;}fields_started_=true;return true;}
    template<class T,class F>Status item(std::string_view name,T& value,F visit){
        if constexpr(Shape!=ProjectionRecordShape::object_presence_sequence&&(std::is_same_v<T,StableId>||std::is_same_v<T,Integer>||std::is_same_v<T,bool>||std::is_same_v<T,std::uint64_t>||(Shape==ProjectionRecordShape::exact_scalar_null_dictionary&&std::is_enum_v<T>))){
            ProjectionRecordArchive archive(writer_);archive.scalar_=true;archive.scalar_name_=name;return visit(archive,value);
        }else return writer_.object(name,[&](auto& record){ProjectionRecordArchive archive(record);return visit(archive,value);});
    }
};
using ActorFoundationProjectionArchive=ProjectionRecordArchive<ProjectionRecordShape::scalar_null_dictionary>;
// exact_fields::JsonCodec also emits enum optionals as scalar values.
using ExactStructureProjectionArchive=ProjectionRecordArchive<ProjectionRecordShape::exact_scalar_null_dictionary>;
using DynamicProjectionArchive=ProjectionRecordArchive<ProjectionRecordShape::object_presence_sequence>;
// callback必须沿原reading()==false分支只借用；不得在写端修复/归一化已发布业务对象。
template<class Archive,class T,class F>Status emit_projection_record(ProjectionWriter& sink,std::string_view name,const T& value,F visit){return sink.object(name,[&](auto& child){Archive archive(child);return visit(archive,const_cast<T&>(value));});}
}
