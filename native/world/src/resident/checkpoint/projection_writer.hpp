// 原JSON作者/冷导出writer的单一输出面；shape sink遍历完全相同分支但不构造值树。
#pragma once
#include "codec_visitor.hpp"
#include "checkpoint_job_memory.hpp"
#include <string_view>
#include <array>
#include <type_traits>
namespace hanmo::resident {
struct BoundaryValue;
// 有限借用的原FieldOrder加两个编译期扩展键；调用期原表/字符须保持存活。
// 空原表保持原无序语义，此时不得追加键；sink frame按值持这个小view。
struct ProjectionFieldOrderView {
    const FieldOrder* original=nullptr;
    std::array<std::string_view,2> appended{};
    std::size_t appended_count=0;
};
// 观察器只接已经通过同sink结构门的字段事件；不持有值，不授schema成功。
class ProjectionObservation {
public:
    virtual ~ProjectionObservation()=default;
    virtual Status field_seen(std::size_t depth,std::string_view parent,std::string_view name)=0;
};
class ProjectionWriter {
public:
    virtual ~ProjectionWriter()=default;
    bool reading()const noexcept{return false;}
    Status status()const{return result_;}
    Status reject(std::string_view,std::string_view path={});
    Status field(std::string_view,Integer);
    // Explicit schema-owned unsigned integer; JSON output stays a number token.
    Status unsigned_field(std::string_view,std::uint64_t);
    Status field(std::string_view,double);
    Status field(std::string_view,bool);
    Status field(std::string_view,const StableId&);
    // 借用文本仅在本次scalar调用有效，sink不得保留源地址。
    Status text(std::string_view,std::string_view);
    Status null(std::string_view);
    // 当前object的原内层FieldOrder仍先核验，外壳可以随后使用自己的原排序。
    Status order_fields(const FieldOrder&);
    template<class Order,std::enable_if_t<std::is_same_v<std::decay_t<Order>,ProjectionFieldOrderView>,int> =0>
    Status order_fields(Order&& order){if(result_.ok)accept(reorder_object(order));return result_;}
    template<class Order,class Fn,std::enable_if_t<std::is_same_v<std::decay_t<Order>,ProjectionFieldOrderView>,int> =0>
    Status object(std::string_view name,Order&& order,Fn emit){
        if(!result_.ok)return result_;accept(begin_object(name,order));if(!result_.ok)return result_;accept(emit(*this));if(result_.ok)accept(end_object());return result_;
    }
    template<class Fn>Status object(std::string_view name,const FieldOrder& order,Fn emit){return object(name,ProjectionFieldOrderView{&order,{},0},std::move(emit));}
    template<class Fn>Status object(std::string_view name,Fn emit){static const FieldOrder empty;return object(name,empty,std::move(emit));}
    template<class Range,class Fn>Status sequence(std::string_view name,const Range& rows,Fn emit){
        if(!result_.ok)return result_;accept(begin_array(name,rows.size()));if(!result_.ok)return result_;for(const auto& row:rows){accept(emit(*this,row));if(!result_.ok)return result_;}accept(end_array());return result_;
    }
    template<class T,class Fn>Status optional(std::string_view name,const std::optional<T>& value,Fn emit){if(value)return object(name,[&](auto& w){return emit(w,*value);});return null(name);}
    // 多域facts/action_params按原顺序向一个dictionary发射；同名冲突由同sink拒绝。
    template<class Fn>Status keyed_dictionary(std::string_view name,const FieldOrder& order,Fn emit){return object(name,order,std::move(emit));}
    // 原组合writer有些先校验独立整段再抽其事实；分支借同job/同字段writer，不向父输出添伪字段。
    // 回调只在此同步调用期借用，不存std::function或拥有捕获。
    template<class Fn>Status validate_detached(Fn&& emit){
        using F=std::remove_reference_t<Fn>;if(result_.ok)accept(validate_branch([](ProjectionWriter& out,const void* value)->Status{return (*static_cast<const F*>(value))(out);},&emit));return result_;
    }
    virtual Status finish()=0;
protected:
    using ValidationEmit=Status(*)(ProjectionWriter&,const void*);
    virtual Status validate_branch(ValidationEmit,const void*)=0;
    using Scalar=std::variant<std::monostate,Integer,double,bool,std::string_view,std::uint64_t>;
    Status result_{true,{},{}};
    void accept(Status value){if(result_.ok&&!value.ok)result_=std::move(value);}
    virtual Status scalar(std::string_view,const Scalar&)=0;
    virtual Status begin_object(std::string_view,ProjectionFieldOrderView)=0;
    virtual Status end_object()=0;
    virtual Status reorder_object(ProjectionFieldOrderView)=0;
    virtual Status begin_array(std::string_view,std::size_t)=0;
    virtual Status end_array()=0;
};
// 只保当前嵌套frame的真实字段名引用；动态dictionary索引按同job实际allocator收费。
class ProjectionShapeWriter final:public ProjectionWriter {
public:
    ProjectionShapeWriter(const CapacityProfile&,std::string_view domain,CheckpointJobBudget&,ProjectionObservation* observation=nullptr);
    ~ProjectionShapeWriter()override;
    Status finish()override;
private:
    struct Impl;CheckpointUniquePtr<Impl> impl_;
    Status validate_branch(ValidationEmit,const void*)override;
    Status scalar(std::string_view,const Scalar&)override;
    Status begin_object(std::string_view,ProjectionFieldOrderView)override;
    Status end_object()override;
    Status reorder_object(ProjectionFieldOrderView)override;
    Status begin_array(std::string_view,std::size_t)override;
    Status end_array()override;
};
// 原旧export使用此sink；同一个emit函数保原FieldOrder、字段值与条件，不复制既有BoundaryValue。
class BoundaryProjectionWriter final:public ProjectionWriter {
public:
    explicit BoundaryProjectionWriter(BoundaryValue& output);
    ~BoundaryProjectionWriter()override;
    Status finish()override;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
    Status validate_branch(ValidationEmit,const void*)override;
    Status scalar(std::string_view,const Scalar&)override;
    Status begin_object(std::string_view,ProjectionFieldOrderView)override;
    Status end_object()override;
    Status reorder_object(ProjectionFieldOrderView)override;
    Status begin_array(std::string_view,std::size_t)override;
    Status end_array()override;
};
}
