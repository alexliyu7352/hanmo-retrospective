// The existing Social adoption-prefix table, shared with actual military
// boundaries. Business consumers supply their original SourceRef field reader
// and error mapping; Common alone verifies the actual append prefix.
#pragma once
#include "common_fact_prefix.hpp"
namespace hanmo::resident::common_fact_prefix_fields {
template<class C,class SourceFields>
Status fields(C& c,CommonActualFactPrefix& value,SourceFields source_fields,
              const char* invalid_code="InvalidCommonFactPrefix"){
    c.field("common_fact_count",value.common_fact_count);
    c.optional("last_common_fact",value.last_common_fact,
        [&](auto& child,auto& source){return source_fields(child,source);});
    if(c.status().ok&&(value.common_fact_count<0||
       bool(value.common_fact_count)!=bool(value.last_common_fact)))
        return c.reject(invalid_code);
    return c.status();
}
}
