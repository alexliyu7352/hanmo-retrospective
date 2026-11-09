// Mechanical Common append-prefix identity, lifted from the original S1
// history reader. The adopting business owner alone proves when and why this
// prefix was captured. This type grants no permission or historical M.
#pragma once
#include "source_ref.hpp"
#include <cstddef>
#include <optional>
#include <type_traits>
namespace hanmo::resident {
class CommonStore;struct CommonState;struct FactRecord;
struct CommonActualFactPrefix {
    Integer common_fact_count=0;
    std::optional<SourceRef> last_common_fact;
};
// Original S1 count/anchor/head/first-append checks, with no success-path copy.
// Consumers retaining count must pin the entire actual prefix, not only last.
Outcome<std::size_t> read_actual_common_fact_prefix(
    const CommonStore&,const CommonActualFactPrefix&);
// Borrow only the current real Common prefix. The caller owns any subsequent
// persistent field copies and their existing memory reservation.
struct CurrentCommonFactPrefix {
    Integer common_fact_count=0;
    const FactRecord* last_common_fact=nullptr;
};
Outcome<CurrentCommonFactPrefix> read_current_common_fact_prefix(const CommonStore&);
// Borrow the strict predecessor prefix of one actual retained Common head.
// The business owner must independently prove that this head immediately
// follows its original check; this mechanical read supplies no authority or M.
Outcome<CurrentCommonFactPrefix> read_actual_common_fact_prefix_before(
    const CommonStore&,const SourceRef&);
// The borrowed header must still belong to the same unchanged table storage.
// Reacquire the current prefix after any table mutation; only the scalar count
// may be retained across mutation when selecting a later actual suffix.
Outcome<std::size_t> read_actual_common_fact_prefix(
    const CommonStore&,const CurrentCommonFactPrefix&);
// Zero-based position in the sole actual Common append order. These mechanical
// reads prove the real header and order only, not domain authority or time.
// State-borrowing original owners use the same header/order proof directly.
Outcome<std::size_t> read_actual_common_fact_position(const CommonState&,const SourceRef&);
Outcome<std::size_t> read_actual_common_fact_position(const CommonStore&,const SourceRef&);
namespace common_fact_prefix_detail {
Outcome<std::size_t> read_borrowed_position(const CommonState&,const SourceRefBuildView&);
Outcome<std::size_t> read_borrowed_position(const CommonState&,const FactRecord&);
Outcome<std::size_t> read_borrowed_position(const CommonStore&,const SourceRefBuildView&);
Outcome<std::size_t> read_borrowed_position(const CommonStore&,const FactRecord&);
Outcome<CurrentCommonFactPrefix> read_borrowed_prefix_before(const CommonStore&,const SourceRefBuildView&);
Outcome<CurrentCommonFactPrefix> read_borrowed_prefix_before(const CommonStore&,const FactRecord&);
}
// Finite type deduction keeps existing {domain,kind,id} calls unambiguously
// SourceRef. Only actual already-typed borrowed views/headers use this route;
// no unknown-type fallback and no alternate source-order implementation.
template<class T,std::enable_if_t<std::is_same_v<T,SourceRefBuildView>||std::is_same_v<T,FactRecord>,int> =0>
Outcome<std::size_t> read_actual_common_fact_position(const CommonState& common,const T& source){
    return common_fact_prefix_detail::read_borrowed_position(common,source);
}
template<class T,std::enable_if_t<std::is_same_v<T,SourceRefBuildView>||std::is_same_v<T,FactRecord>,int> =0>
Outcome<std::size_t> read_actual_common_fact_position(const CommonStore& common,const T& source){
    return common_fact_prefix_detail::read_borrowed_position(common,source);
}
template<class T,std::enable_if_t<std::is_same_v<T,SourceRefBuildView>||std::is_same_v<T,FactRecord>,int> =0>
Outcome<CurrentCommonFactPrefix> read_actual_common_fact_prefix_before(const CommonStore& common,const T& source){
    return common_fact_prefix_detail::read_borrowed_prefix_before(common,source);
}
Outcome<bool> actual_common_fact_in_prefix(
    const CommonStore&,const SourceRef&,const CommonActualFactPrefix&);
class CommonActualFactPrefixSink {
public:
    virtual ~CommonActualFactPrefixSink()=default;
    virtual Status actual_prefix_fact(std::size_t position,const FactRecord&)=0;
};
Status visit_actual_common_fact_prefix(
    const CommonStore&,const CommonActualFactPrefix&,CommonActualFactPrefixSink&);
Status visit_actual_common_fact_prefix(
    const CommonStore&,const CurrentCommonFactPrefix&,CommonActualFactPrefixSink&);
}
