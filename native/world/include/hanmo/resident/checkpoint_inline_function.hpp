// compiled目录回调只持固定inline值；拥有集合必须作为真实runtime字段，不能藏在std::function堆捕获。
#pragma once
#include <cstddef>
#include <functional>
#include <new>
#include <type_traits>
#include <utility>
namespace hanmo::resident {
template<class Signature>class CheckpointInlineFunction;
template<class R,class... A>class CheckpointInlineFunction<R(A...)> {
public:
    CheckpointInlineFunction()noexcept=default;
    CheckpointInlineFunction(std::nullptr_t)noexcept{}
    CheckpointInlineFunction(const CheckpointInlineFunction& other)noexcept{copy_from(other);}
    CheckpointInlineFunction(CheckpointInlineFunction&& other)noexcept{copy_from(other);other.reset();}
    template<class F,std::enable_if_t<!std::is_same_v<std::decay_t<F>,CheckpointInlineFunction>,int> =0>
    CheckpointInlineFunction(F&& function){assign(std::forward<F>(function));}
    CheckpointInlineFunction& operator=(const CheckpointInlineFunction& other)noexcept{if(this!=&other){reset();copy_from(other);}return *this;}
    CheckpointInlineFunction& operator=(CheckpointInlineFunction&& other)noexcept{if(this!=&other){reset();copy_from(other);other.reset();}return *this;}
    CheckpointInlineFunction& operator=(std::nullptr_t)noexcept{reset();return *this;}
    template<class F,std::enable_if_t<!std::is_same_v<std::decay_t<F>,CheckpointInlineFunction>,int> =0>
    CheckpointInlineFunction& operator=(F&& function){CheckpointInlineFunction next(std::forward<F>(function));*this=std::move(next);return *this;}
    ~CheckpointInlineFunction()noexcept{reset();}
    explicit operator bool()const noexcept{return invoke_!=nullptr;}
    R operator()(A... arguments)const{if(!invoke_)throw std::bad_function_call();return invoke_(storage_,std::forward<A>(arguments)...);}
private:
    // 用具名placement-copy建立每个目标的真实对象寿命，不靠字节复制冒C++17对象构造。
    static constexpr std::size_t capacity=32;
    alignas(std::max_align_t) unsigned char storage_[capacity]{};
    R(*invoke_)(const void*,A&&...)=nullptr;
    void(*copy_)(void*,const void*)noexcept=nullptr;
    void(*destroy_)(void*)noexcept=nullptr;
    void reset()noexcept{if(destroy_)destroy_(storage_);invoke_=nullptr;copy_=nullptr;destroy_=nullptr;}
    void copy_from(const CheckpointInlineFunction& other)noexcept{if(!other.invoke_)return;other.copy_(storage_,other.storage_);invoke_=other.invoke_;copy_=other.copy_;destroy_=other.destroy_;}
    template<class F>void assign(F&& function){
        using Value=std::decay_t<F>;
        static_assert(std::is_trivially_copyable_v<Value>&&std::is_nothrow_copy_constructible_v<Value>,"Compiled callback must keep owning values in typed runtime fields");
        static_assert(std::is_trivially_destructible_v<Value>&&sizeof(Value)<=capacity&&alignof(Value)<=alignof(std::max_align_t),"Compiled callback exceeds fixed inline storage");
        static_assert(std::is_invocable_r_v<R,const Value&,A...>,"Compiled callback must be const callable");
        if constexpr(std::is_pointer_v<Value>){if(!function)return;}
        ::new(static_cast<void*>(storage_))Value(std::forward<F>(function));
        invoke_=[](const void* value,A&&... arguments)->R{return std::invoke(*static_cast<const Value*>(value),std::forward<A>(arguments)...);};
        copy_=[](void* into,const void* from)noexcept{::new(into)Value(*static_cast<const Value*>(from));};
        destroy_=[](void* value)noexcept{static_cast<Value*>(value)->~Value();};
    }
};
} // 回调字段自身随runtime真实allocation计费，无独立heap或后台world捕获。
