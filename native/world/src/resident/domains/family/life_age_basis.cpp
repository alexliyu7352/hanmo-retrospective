// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
#include "life_age_basis.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
namespace hanmo::resident {
bool same_life_age_basis_ref(const LifeAgeBasisRef& a,const LifeAgeBasisRef& b){if(a.index()!=b.index())return false;if(const auto* x=std::get_if<BirthFactRef>(&a))return x->birth_fact_id==std::get<BirthFactRef>(b).birth_fact_id;const auto& x=std::get<DeclaredAgeBasisRef>(a);const auto& y=std::get<DeclaredAgeBasisRef>(b);return x.kind==y.kind&&x.actor_id==y.actor_id&&x.source_ref==y.source_ref;}
Status visit_life_age_basis_ref(CodecVisitor& c,LifeAgeBasisRef& value){return life_age_basis_fields(c,value);}
Status emit_life_age_basis_ref(ProjectionWriter& w,std::string_view name,const LifeAgeBasisRef& value){return emit_projection_record<ActorFoundationProjectionArchive>(w,name,value,[](auto& c,auto& v){return life_age_basis_fields(c,v);});}
Outcome<LifeAgeBasisRef> decode_life_age_basis_ref(const BoundaryValue& value){LifeAgeBasisRef out;ActorFoundationJsonVisitor c(value);auto valid=life_age_basis_fields(c,out);if(valid.ok)valid=c.finish();return {valid.ok,std::move(out),valid.error};}
Status visit_family_decision_age_sources4(CodecVisitor& c,FamilyDecisionAgeSources4& value){return family_decision_age_sources4_fields(c,value);}
Status emit_family_decision_age_sources4(ProjectionWriter& w,std::string_view name,const FamilyDecisionAgeSources4& value){return emit_projection_record<ActorFoundationProjectionArchive>(w,name,value,[](auto& c,auto& v){return family_decision_age_sources4_fields(c,v);});}
Outcome<FamilyDecisionAgeSources4> decode_family_decision_age_sources4(const BoundaryValue& value){FamilyDecisionAgeSources4 out;ActorFoundationJsonVisitor c(value);auto valid=family_decision_age_sources4_fields(c,out);if(valid.ok)valid=c.finish();return {valid.ok,std::move(out),valid.error};}
}
