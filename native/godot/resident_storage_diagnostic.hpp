// Original storage reply payloads keep their diagnostics alive through every poll copy.
// No global Error/Status or on-wire error name is changed by this local owning view.
#pragma once
#include "resident_slot_media.hpp"
#include "hanmo/resident/model_contract.hpp"
#include <array>
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>

namespace hanmo::resident::storage {
class StorageDiagnostic {
    struct LongText;
    std::array<char,96> inline_{};
    std::size_t inline_size_=0;
    std::shared_ptr<const LongText> long_;
    std::optional<SlotMediaError> media_;
public:
    StorageDiagnostic()noexcept=default;
    StorageDiagnostic(std::string_view);
    StorageDiagnostic(const std::string& value):StorageDiagnostic(std::string_view(value)){}
    StorageDiagnostic(const char* value):StorageDiagnostic(std::string_view(value)){}
    StorageDiagnostic(const StorageDiagnostic&)noexcept=default;
    StorageDiagnostic(StorageDiagnostic&&)noexcept=default;
    StorageDiagnostic& operator=(const StorageDiagnostic&)noexcept=default;
    StorageDiagnostic& operator=(StorageDiagnostic&&)noexcept=default;
    // runtime_error's original immutable diagnostic is retained, never copied into a second string.
    // Its initial legacy exception allocation remains the media producer's responsibility.
    static StorageDiagnostic from_media(const SlotMediaError&)noexcept;
    const char* c_str()const noexcept;
    std::string_view view()const noexcept;
    bool empty()const noexcept{return view().empty();}
    std::size_t size()const noexcept{return view().size();}
    void clear()noexcept{*this=StorageDiagnostic{};}
    void retain(CheckpointRetentionCensus&)const;
    template<class T,std::enable_if_t<std::is_convertible_v<const T&,std::string_view>,int> =0>
    friend bool operator==(const StorageDiagnostic& a,const T& b)noexcept{return a.view()==std::string_view(b);}
    template<class T,std::enable_if_t<std::is_convertible_v<const T&,std::string_view>,int> =0>
    friend bool operator!=(const StorageDiagnostic& a,const T& b)noexcept{return a.view()!=std::string_view(b);}
    template<class T,std::enable_if_t<std::is_convertible_v<const T&,std::string_view>,int> =0>
    friend bool operator==(const T& a,const StorageDiagnostic& b)noexcept{return std::string_view(a)==b.view();}
    template<class T,std::enable_if_t<std::is_convertible_v<const T&,std::string_view>,int> =0>
    friend bool operator!=(const T& a,const StorageDiagnostic& b)noexcept{return std::string_view(a)!=b.view();}
    friend bool operator==(const StorageDiagnostic& a,const StorageDiagnostic& b)noexcept{return a.view()==b.view();}
    friend bool operator!=(const StorageDiagnostic& a,const StorageDiagnostic& b)noexcept{return a.view()!=b.view();}
};
// Legacy core errors are not flattened or rewritten. Only the original storage cache's
// ownership changes: the first copy is reserved before construction, subsequent polls borrow it.
class StorageCoreError {
    struct State;std::shared_ptr<const State> state_;
public:
    StorageCoreError()noexcept=default;
    StorageCoreError(const RuleError&);
    const RuleError* get()const noexcept;
    const RuleError* operator->()const noexcept{return get();}
    const RuleError& operator*()const noexcept{return *get();}
    explicit operator bool()const noexcept{return bool(state_);}
    void reset()noexcept{state_.reset();}
    void retain(CheckpointRetentionCensus&)const;
};
static_assert(std::is_nothrow_copy_constructible_v<SlotMediaError>);
static_assert(std::is_nothrow_copy_assignable_v<SlotMediaError>);
static_assert(std::is_nothrow_copy_constructible_v<StorageDiagnostic>);
static_assert(std::is_nothrow_copy_assignable_v<StorageDiagnostic>);
static_assert(std::is_nothrow_copy_constructible_v<StorageCoreError>);
}
