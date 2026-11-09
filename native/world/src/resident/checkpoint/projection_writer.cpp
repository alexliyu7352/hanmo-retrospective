#include "projection_writer.hpp"
#include "ordered_value.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace hanmo::resident {
namespace {
Status bad(const StableId& code){return {false,{},{{code,(code=="BoundaryFieldOrderMismatch"||code=="DuplicateBoundaryKey")?"boundary":"projection"},ErrorKind::rule,{},{},{},{}}};}
bool valid_order(ProjectionFieldOrderView order) noexcept {
    if(order.appended_count>order.appended.size())return false;
    const auto original_size=order.original?order.original->size():0;
    if(original_size==0)return order.appended_count==0;
    if(original_size>std::numeric_limits<std::size_t>::max()-order.appended_count)return false;
    for(std::size_t i=0;i<order.appended_count;++i)if(order.appended[i].empty())return false;
    return true;
}
std::size_t order_size(ProjectionFieldOrderView order) noexcept {return (order.original?order.original->size():0)+order.appended_count;}
std::string_view order_key(ProjectionFieldOrderView order,std::size_t i) noexcept {
    const auto original_size=order.original?order.original->size():0;
    return i<original_size?std::string_view((*order.original)[i]):order.appended[i-original_size];
}
// 原无尾路径沿既有排序；有限尾先全核再原地置换，不复制FieldOrder/键索引。
Status apply_projection_order(BoundaryValue& value,ProjectionFieldOrderView order){
    if(!valid_order(order))return bad("BoundaryFieldOrderMismatch");
    auto* rows=value.as<BoundaryValue::Dict>();if(!rows)return bad("ExpectedBoundaryDictionary");
    const auto count=order_size(order);if(count==0)return {true,{},{}};
    if(order.appended_count==0)return apply_field_order(value,*order.original);
    if(count!=rows->size())return bad("BoundaryFieldOrderMismatch");
    for(std::size_t i=0;i<rows->size();++i)for(std::size_t j=0;j<i;++j)if((*rows)[i].first==(*rows)[j].first)return bad("DuplicateBoundaryKey");
    for(std::size_t i=0;i<count;++i){
        const auto key=order_key(order,i);
        if(std::find_if(rows->begin(),rows->end(),[&](const auto& row){return row.first==key;})==rows->end())return bad("BoundaryFieldOrderMismatch");
        for(std::size_t j=0;j<i;++j)if(order_key(order,j)==key)return bad("BoundaryFieldOrderMismatch");
    }
    for(std::size_t i=0;i<count;++i){
        const auto key=order_key(order,i);
        auto found=std::find_if(rows->begin()+i,rows->end(),[&](const auto& row){return row.first==key;});
        if(found!=rows->begin()+i)std::iter_swap(rows->begin()+i,found);
    }
    return {true,{},{}};
}
}
Status ProjectionWriter::reject(std::string_view code,std::string_view path){if(result_.ok)result_={false,{},{{StableId(code),StableId(path)},ErrorKind::rule,{},{},{},{}}};return result_;}
Status ProjectionWriter::field(std::string_view name,Integer value){if(result_.ok)accept(scalar(name,Scalar{value}));return result_;}
Status ProjectionWriter::unsigned_field(std::string_view name,std::uint64_t value){if(result_.ok)accept(scalar(name,Scalar{value}));return result_;}
Status ProjectionWriter::field(std::string_view name,double value){if(result_.ok){if(!std::isfinite(value))return reject("NonFiniteBoundaryValue");accept(scalar(name,Scalar{value}));}return result_;}
Status ProjectionWriter::field(std::string_view name,bool value){if(result_.ok)accept(scalar(name,Scalar{value}));return result_;}
Status ProjectionWriter::field(std::string_view name,const StableId& value){if(result_.ok)accept(scalar(name,Scalar{std::string_view(value)}));return result_;}
Status ProjectionWriter::text(std::string_view name,std::string_view value){if(result_.ok)accept(scalar(name,Scalar{value}));return result_;}
Status ProjectionWriter::order_fields(const FieldOrder& order){return order_fields(ProjectionFieldOrderView{&order,{},0});}
Status ProjectionWriter::null(std::string_view name){if(result_.ok)accept(scalar(name,Scalar{std::monostate{}}));return result_;}
struct ProjectionShapeWriter::Impl {
    struct Frame {bool array=false;std::string_view name;ProjectionFieldOrderView order{};std::size_t expected=0,seen=0;using Key=std::basic_string<char,std::char_traits<char>,CheckpointAllocator<char>>;CheckpointVector<Key> keys;explicit Frame(std::shared_ptr<CheckpointMemoryResource> r):keys(CheckpointAllocator<Key>{std::move(r)}){}};
    const CapacityProfile& profile;CheckpointJobBudget& job;std::shared_ptr<CheckpointMemoryResource> memory;CheckpointVector<Frame> stack;ProjectionObservation* observation=nullptr;bool root=false;
    Impl(const CapacityProfile& p,std::string_view d,CheckpointJobBudget& j,std::shared_ptr<CheckpointMemoryResource> m,ProjectionObservation* observer):profile(p),job(j),memory(std::move(m)),stack(CheckpointAllocator<Frame>{memory}),observation(observer){}
    Status value(std::string_view name){if(stack.empty()){if(root||!name.empty())return bad("InvalidProjectionRoot");root=true;return {true,{},{}};}auto& parent=stack.back();if(parent.array){if(!name.empty()||parent.seen==parent.expected)return bad("ProjectionArrayBoundary");++parent.seen;return {true,{},{}};}if(name.empty()||std::find_if(parent.keys.begin(),parent.keys.end(),[&](const auto& key){return std::string_view(key.data(),key.size())==name;})!=parent.keys.end())return bad("DuplicateBoundaryKey");parent.keys.emplace_back(name.data(),name.size(),CheckpointAllocator<char>{memory});if(observation)return observation->field_seen(stack.size(),parent.name,name);return {true,{},{}};}
};
ProjectionShapeWriter::ProjectionShapeWriter(const CapacityProfile& p,std::string_view d,CheckpointJobBudget& job,ProjectionObservation* observation){auto checked=job.check_profile(p);if(!checked.ok)throw checkpoint_job_detail::FailureException{std::move(checked.error)};auto r=checkpoint_memory_resource(job,CheckpointJobUse::scratch);if(!r.ok)throw checkpoint_job_detail::FailureException{std::move(r.error)};ScopedCheckpointMemory scope(r.value);impl_=checkpoint_make_unique<Impl>(p,d,job,std::move(r.value),observation);}
ProjectionShapeWriter::~ProjectionShapeWriter()=default;
Status ProjectionShapeWriter::validate_branch(ValidationEmit emit,const void* value){static const StableId domain;ProjectionShapeWriter child(impl_->profile,domain,impl_->job);auto result=child.object("",[&](auto& out){return emit(out,value);});return result.ok?child.finish():result;}
Status ProjectionShapeWriter::scalar(std::string_view name,const Scalar&){return impl_->value(name);}
Status ProjectionShapeWriter::begin_object(std::string_view name,ProjectionFieldOrderView order){if(!valid_order(order))return bad("BoundaryFieldOrderMismatch");auto s=impl_->value(name);if(!s.ok)return s;if(impl_->stack.size()>=impl_->profile.max_nesting_depth)return checkpoint_capacity_failure("projection_depth",impl_->stack.size()+1,impl_->profile.profile_id,impl_->profile.revision);Impl::Frame frame(impl_->memory);frame.name=name;frame.order=order;impl_->stack.push_back(std::move(frame));return {true,{},{}};}
Status ProjectionShapeWriter::reorder_object(ProjectionFieldOrderView order){if(impl_->stack.empty()||impl_->stack.back().array)return bad("InvalidProjectionBoundary");if(!valid_order(order))return bad("BoundaryFieldOrderMismatch");const auto& keys=impl_->stack.back().keys;const auto count=order_size(order);if(count!=0){if(count!=keys.size())return bad("BoundaryFieldOrderMismatch");for(std::size_t i=0;i<count;++i){const auto key=order_key(order,i);if(std::find_if(keys.begin(),keys.end(),[&](const auto& found){return std::string_view(found.data(),found.size())==key;})==keys.end())return bad("BoundaryFieldOrderMismatch");for(std::size_t j=0;j<i;++j)if(order_key(order,j)==key)return bad("BoundaryFieldOrderMismatch");}}return {true,{},{}};}
Status ProjectionShapeWriter::end_object(){if(impl_->stack.empty()||impl_->stack.back().array)return bad("InvalidProjectionBoundary");auto status=reorder_object(impl_->stack.back().order);if(status.ok)impl_->stack.pop_back();return status;}
Status ProjectionShapeWriter::begin_array(std::string_view name,std::size_t count){auto s=impl_->value(name);if(!s.ok)return s;if(impl_->stack.size()>=impl_->profile.max_nesting_depth)return checkpoint_capacity_failure("projection_depth",impl_->stack.size()+1,impl_->profile.profile_id,impl_->profile.revision);Impl::Frame frame(impl_->memory);frame.name=name;frame.array=true;frame.expected=count;impl_->stack.push_back(std::move(frame));return {true,{},{}};}
Status ProjectionShapeWriter::end_array(){if(impl_->stack.empty()||!impl_->stack.back().array||impl_->stack.back().seen!=impl_->stack.back().expected)return bad("ProjectionArrayBoundary");impl_->stack.pop_back();return {true,{},{}};}
Status ProjectionShapeWriter::finish(){if(result_.ok&&(!impl_->root||!impl_->stack.empty()))return reject("InvalidProjectionBoundary");return result_;}
struct BoundaryProjectionWriter::Impl {
    struct Frame {BoundaryValue* value=nullptr;ProjectionFieldOrderView order{};std::size_t expected=0;bool array=false;};
    BoundaryValue& output;std::vector<Frame> stack;bool root=false;explicit Impl(BoundaryValue& o):output(o){}
    // 仅失败侧从现有真实父子地址反查键路径，不给成功输出增加字段或持有借用名字。
    void annotate_duplicate(RuleError& error,std::string_view name)const{
        try{
        StableId path=error.original.path;
        const auto append_key=[&](std::string_view key){path+='/';for(char ch:key){if(ch=='~')path+="~0";else if(ch=='/')path+="~1";else path+=ch;}};
        for(std::size_t depth=1;depth<stack.size();++depth){
            const auto* child=stack[depth].value;const auto* parent=stack[depth-1].value;
            if(const auto* rows=parent->as<BoundaryValue::Dict>()){
                for(const auto& row:*rows)if(&row.second==child){append_key(row.first);break;}
            }else if(const auto* rows=parent->as<BoundaryValue::Array>()){
                for(std::size_t i=0;i<rows->size();++i)if(&(*rows)[i]==child){append_key(std::to_string(i));break;}
            }
        }
        append_key(name);error.original.path.swap(path);
        }catch(...){/* 诊断分配失败保留权威原错误，不把失败改成另一个业务结果。 */}
    }
    // 原排序先判失败；只给实际重复首键补位置，不重排或吞掉错误。
    Status ordered(ProjectionFieldOrderView order){
        auto result=apply_projection_order(*stack.back().value,order);
        if(!result.ok&&result.error.original.code=="DuplicateBoundaryKey"){
            if(const auto* rows=stack.back().value->as<BoundaryValue::Dict>())
                for(std::size_t i=0;i<rows->size();++i)for(std::size_t j=0;j<i;++j)
                    if((*rows)[i].first==(*rows)[j].first){annotate_duplicate(result.error,(*rows)[i].first);return result;}
        }
        return result;
    }
    Outcome<BoundaryValue*> append(std::string_view name,BoundaryValue value){if(stack.empty()){if(root||!name.empty())return {false,nullptr,bad("InvalidProjectionRoot").error};root=true;output=std::move(value);return {true,&output,{}};}auto& frame=stack.back();if(frame.array){auto& rows=*frame.value->as<BoundaryValue::Array>();if(!name.empty()||rows.size()>=frame.expected)return {false,nullptr,bad("ProjectionArrayBoundary").error};rows.push_back(std::move(value));return {true,&rows.back(),{}};}auto& dict=*frame.value->as<BoundaryValue::Dict>();for(const auto& row:dict)if(row.first==name){auto result=bad("DuplicateBoundaryKey");annotate_duplicate(result.error,name);return {false,nullptr,std::move(result.error)};}dict.emplace_back(StableId(name),std::move(value));return {true,&dict.back().second,{}};}
};
BoundaryProjectionWriter::BoundaryProjectionWriter(BoundaryValue& out):impl_(std::make_unique<Impl>(out)){}
BoundaryProjectionWriter::~BoundaryProjectionWriter()=default;
Status BoundaryProjectionWriter::validate_branch(ValidationEmit emit,const void* value){BoundaryValue discarded;BoundaryProjectionWriter child(discarded);auto result=child.object("",[&](auto& out){return emit(out,value);});return result.ok?child.finish():result;}
Status BoundaryProjectionWriter::scalar(std::string_view name,const Scalar& value){BoundaryValue out;std::visit([&](const auto& v){using T=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<T,std::monostate>)out=BoundaryValue{};else if constexpr(std::is_same_v<T,std::string_view>)out=BoundaryValue{StableId(v)};else out=BoundaryValue{v};},value);auto appended=impl_->append(name,std::move(out));return appended.ok?Status{true,{},{}}:Status{false,{},appended.error};}
Status BoundaryProjectionWriter::begin_object(std::string_view name,ProjectionFieldOrderView order){if(!valid_order(order))return bad("BoundaryFieldOrderMismatch");auto value=impl_->append(name,BoundaryValue{BoundaryValue::Dict{}});if(!value.ok)return {false,{},value.error};impl_->stack.push_back({value.value,order,0,false});return {true,{},{}};}
Status BoundaryProjectionWriter::reorder_object(ProjectionFieldOrderView order){if(impl_->stack.empty()||impl_->stack.back().array)return bad("InvalidProjectionBoundary");return impl_->ordered(order);}
Status BoundaryProjectionWriter::end_object(){if(impl_->stack.empty()||impl_->stack.back().array)return bad("InvalidProjectionBoundary");auto frame=impl_->stack.back();auto s=impl_->ordered(frame.order);if(s.ok)impl_->stack.pop_back();return s;}
Status BoundaryProjectionWriter::begin_array(std::string_view name,std::size_t count){auto value=impl_->append(name,BoundaryValue{BoundaryValue::Array{}});if(!value.ok)return {false,{},value.error};value.value->as<BoundaryValue::Array>()->reserve(count);impl_->stack.push_back({value.value,{},count,true});return {true,{},{}};}
Status BoundaryProjectionWriter::end_array(){if(impl_->stack.empty()||!impl_->stack.back().array||impl_->stack.back().value->as<BoundaryValue::Array>()->size()!=impl_->stack.back().expected)return bad("ProjectionArrayBoundary");impl_->stack.pop_back();return {true,{},{}};}
Status BoundaryProjectionWriter::finish(){if(result_.ok&&(!impl_->root||!impl_->stack.empty()))return reject("InvalidProjectionBoundary");return result_;}
}
