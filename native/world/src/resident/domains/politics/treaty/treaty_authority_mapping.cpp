#include "resident/kernel/catalog_section_observation.hpp"
#include "treaty_authority_mapping.hpp"
#include <algorithm>
#include "resident/kernel/original_catalog_identity.hpp"
namespace hanmo::resident {namespace {
bool scope(const ScopeRef& s){return !s.id.empty()&&static_cast<unsigned>(s.kind)<=static_cast<unsigned>(ScopeRef::Kind::activity);}
}
Status validate_treaty_authority_mapping_shape(const TreatyAuthorityMappingCatalog& c){
 for(std::size_t n=0;n<c.definitions.size();++n){const auto&d=c.definitions[n];
  if(d.terms_definition_id.empty()||d.parties.empty())return common_failure("InvalidTreatyAuthorityDefinition",d.terms_definition_id);
  for(std::size_t j=0;j<n;++j)if(c.definitions[j].terms_definition_id==d.terms_definition_id)return common_failure("DuplicateTreatyAuthorityDefinition",d.terms_definition_id);
  for(std::size_t p=0;p<d.parties.size();++p){const auto&a=d.parties[p];
   if(a.party_id.empty()||!scope(a.ratification_scope)||a.terms.empty())return common_failure("InvalidTreatyPartyAuthorityMapping",a.party_id);
   for(std::size_t j=0;j<p;++j)if(d.parties[j].party_id==a.party_id)return common_failure("DuplicateTreatyPartyAuthorityMapping",a.party_id);
   for(std::size_t t=0;t<a.terms.size();++t){const auto&r=a.terms[t];
    if(r.term_id.empty()||r.command_kind.empty()||!scope(r.scope)||static_cast<unsigned>(r.term_kind)>static_cast<unsigned>(TreatyTermKind::submission))return common_failure("InvalidTreatyTermAuthorityMapping",r.term_id);
    for(std::size_t j=0;j<t;++j)if(a.terms[j].term_id==r.term_id)return common_failure("DuplicateTreatyTermAuthorityMapping",r.term_id);
   }
  }
 }return common_success();
}
Status validate_treaty_authority_mapping_catalog(const TreatyAuthorityMappingCatalog& c,const PoliticalTreatyTermsCatalog& terms){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 114, "TreatyAuthorityMappingCatalog", "validate_treaty_authority_mapping_catalog", &c);

 auto valid=validate_treaty_authority_mapping_shape(c);if(!valid.ok)return valid;
 if(c.identity.content_digest.empty()||!same_original_catalog_identity(c.identity,terms.identity))return common_failure("TreatyAuthorityMappingCatalogIdentityMismatch");
 if(c.definitions.size()!=terms.definitions.rows().size())return common_failure("TreatyAuthorityMappingDefinitionCoverageMismatch");
 for(const auto&d:c.definitions){const auto*original=terms.definitions.find(d.terms_definition_id);
  if(!original||original->id!=d.terms_definition_id)return common_failure("TreatyAuthorityMappingDefinitionMissing",d.terms_definition_id);
  valid=validate_treaty_terms_shape(original->terms);if(!valid.ok)return valid;
  if(d.parties.size()!=original->terms.parties.size())return common_failure("TreatyAuthorityMappingPartyCoverageMismatch",d.terms_definition_id);
  for(const auto&p:d.parties){if(std::find(original->terms.parties.begin(),original->terms.parties.end(),p.party_id)==original->terms.parties.end()||p.terms.size()!=original->terms.terms.size())return common_failure("TreatyAuthorityMappingTermCoverageMismatch",p.party_id);
   for(const auto&t:p.terms){auto i=std::find_if(original->terms.terms.begin(),original->terms.terms.end(),[&](const auto&r){return r.term_id==t.term_id;});
    if(i==original->terms.terms.end()||i->terms.index()!=static_cast<std::size_t>(t.term_kind))return common_failure("TreatyAuthorityMappingOriginalKindMismatch",t.term_id);
   }
  }
 }return common_success();
}
Outcome<const TreatyAuthoredPartyAuthority*> read_treaty_authored_authority_mapping(const TreatyAuthorityMappingCatalog& c,const PoliticalTreatyTermsCatalog& terms,const TreatyRevisionView& revision,const StableId& party){
 auto valid=validate_treaty_authority_mapping_catalog(c,terms);if(!valid.ok)return {false,nullptr,valid.error};
 if(!revision.definition||terms.definitions.find(revision.definition->id)!=revision.definition)return {false,nullptr,common_failure("TreatyAuthorityMappingForeignRevision",revision.treaty_id).error};
 for(const auto&d:c.definitions)if(d.terms_definition_id==revision.definition->id)for(const auto&p:d.parties)if(p.party_id==party)return {true,&p,{}};
 return {false,nullptr,common_failure("TreatyAuthorityMappingPartyMissing",party).error};
}
}
