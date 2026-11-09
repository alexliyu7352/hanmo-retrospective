// The existing field visitors drive both original JSON and bounded shape. No second field list.
#pragma once
#include "resident/checkpoint/projection_record_archive.hpp"
#include "task_dynamic_codec.hpp"
namespace hanmo::resident::dynamic_projection {
using Archive=DynamicProjectionArchive;
template<class T,class F>Status emit(ProjectionWriter&sink,std::string_view name,const T&value,F visit){return sink.object(name,[&](ProjectionWriter&child){Archive a(child);return visit(a,const_cast<T&>(value));});}
template<class T,class F>Outcome<BoundaryValue> encode(const T&value,F visit){BoundaryValue result;BoundaryProjectionWriter sink(result);auto status=emit(sink,"",value,visit);if(status.ok)status=sink.finish();return status.ok?Outcome<BoundaryValue>{true,std::move(result),{}}:Outcome<BoundaryValue>{false,{},status.error};}
}
