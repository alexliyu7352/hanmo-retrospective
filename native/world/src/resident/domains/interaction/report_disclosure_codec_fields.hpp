#pragma once
#include "report_disclosure_types.hpp"
namespace hanmo::resident::report_disclosure_codec {
template<class A>Status source(A&a,SourceRef&v){a.field("domain",v.domain);a.field("kind",v.kind);return a.field("id",v.id);}
template<class A>Status moment_source(A&a,MomentOwnerSource&v){a.object("source_ref",[&](auto&c){return source(c,v.source_ref);});return a.field("source_schema",v.source_schema);}
template<class A>Status permission(A&a,ReportDisclosurePermission&v){Integer kind=static_cast<Integer>(v.kind);a.field("kind",kind);if(kind<0||kind>2)return a.reject("InvalidReportDisclosurePermissionKind");if(a.reading())v.kind=static_cast<ReportDisclosureBasisKind>(kind);return a.object("original_source",[&](auto&c){return moment_source(c,v.original_source);});}
}
