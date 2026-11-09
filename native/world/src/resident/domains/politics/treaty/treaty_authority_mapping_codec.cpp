#include "treaty_authority_mapping.hpp"
#include "treaty_terms_codec_fields.hpp"
namespace hanmo::resident {namespace {
template<class C>Status fields(C&c,TreatyAuthorityMappingCatalog&r){
 Integer schema=1;c.field("schema_version",schema);if(!c.status().ok)return c.status();if(schema!=1)return c.reject("UnsupportedTreatyAuthorityMappingSchema");
 c.sequence("definitions",r.definitions,[](auto&v,auto&d){v.field("terms_definition_id",d.terms_definition_id);
  return v.sequence("parties",d.parties,[](auto&w,auto&p){w.field("party_id",p.party_id);w.object("ratification_scope",[&](auto&a){return treaty_terms_codec_fields::scope(a,p.ratification_scope);});
   return w.sequence("terms",p.terms,[](auto&a,auto&t){a.field("term_id",t.term_id);static constexpr const char*tags[]={"truce","passage","mutual_defense","material_aid","submission"};treaty_terms_codec_fields::enum_field(a,"term_kind",t.term_kind,tags);a.field("command_kind",t.command_kind);return a.object("scope",[&](auto&b){return treaty_terms_codec_fields::scope(b,t.scope);});});
  });
 });
 if(c.status().ok){auto valid=validate_treaty_authority_mapping_shape(r);if(!valid.ok)return c.reject(valid.error.original.code);}return c.status();
}
}
Status visit_treaty_authority_mapping(CodecVisitor&c,TreatyAuthorityMappingCatalog&r){return fields(c,r);}
Status emit_treaty_authority_mapping_projection(ProjectionWriter&w,std::string_view name,const TreatyAuthorityMappingCatalog&r){return emit_projection_record<ActorFoundationProjectionArchive>(w,name,r,[](auto&c,auto&v){return fields(c,v);});}
Outcome<BoundaryValue> encode_treaty_authority_mapping(const TreatyAuthorityMappingCatalog&r){BoundaryValue out;BoundaryProjectionWriter w(out);auto valid=emit_treaty_authority_mapping_projection(w,"",r);if(valid.ok)valid=w.finish();return {valid.ok,std::move(out),valid.error};}
Outcome<TreatyAuthorityMappingCatalog> decode_treaty_authority_mapping(const BoundaryValue&input,const CatalogIdentity&identity,const PoliticalTreatyTermsCatalog&terms){TreatyAuthorityMappingCatalog out;out.identity=identity;ActorFoundationJsonVisitor reader(input);auto valid=fields(reader,out);if(valid.ok)valid=reader.finish();if(valid.ok)valid=validate_treaty_authority_mapping_catalog(out,terms);return {valid.ok,std::move(out),valid.error};}
}
