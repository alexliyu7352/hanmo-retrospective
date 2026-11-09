// /2显式布局，与旧/1不猜字段、无player cache来源。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "political_dismissal2.hpp"
#include "resident/domains/organization/organization_lifecycle_codec_detail.hpp"
namespace hanmo::resident {namespace {using namespace organization_lifecycle_fields;
template<class C>Status fields(C& c,PoliticalDismissalSources2& s){c.field("version",s.version);if(c.status().ok&&s.version!=2)return c.reject("PoliticalDismissal2VersionRequired");return table(c,"dismissals",s.dismissals,[](auto& a,auto& r){a.object("dismissal_source",[&](auto& b){return source(b,r.dismissal_source);});a.object("office_change_source",[&](auto& b){return source(b,r.office_change_source);});a.object("authority_basis_ref",[&](auto& b){b.object("issued_source",[&](auto& d){return source(d,r.authority_basis_ref.issued_source);});b.field("source_schema",r.authority_basis_ref.source_schema);return b.status().ok&&r.authority_basis_ref.source_schema!=1?b.reject("AuthorityBasis1Required"):b.status();});return a.field("jurisdiction_revision",r.jurisdiction_revision);});}
}
Status visit_political_dismissal2(CodecVisitor& c,PoliticalDismissalSources2& s){return fields(c,s);}
Status emit_political_dismissal2_projection(ProjectionWriter& w,std::string_view n,const PoliticalDismissalSources2& s){return emit_projection_record<ActorFoundationProjectionArchive>(w,n,s,[](auto& c,auto& v){return fields(c,v);});}
Outcome<BoundaryValue> encode_political_dismissal2(const PoliticalDismissalSources2& s){BoundaryValue out;BoundaryProjectionWriter w(out);auto status=emit_political_dismissal2_projection(w,"",s);if(status.ok)status=w.finish();return {status.ok,std::move(out),status.error};}
Outcome<PoliticalDismissalSources2> decode_political_dismissal2(const BoundaryValue& v){PoliticalDismissalSources2 out;ActorFoundationJsonVisitor c(v);auto status=fields(c,out);if(status.ok)status=c.finish();return {status.ok,std::move(out),status.error};}
}
