#include "resident/kernel/catalog_section_observation.hpp"
#include "native_social_selection.hpp"
#include "exact_time_profile.hpp"
#include "native_catalog_decode.hpp"
#include "original_catalog_identity.hpp"
#include "resident/checkpoint/checkpoint_json_decode.hpp"
#include "resident/domains/social/social_projection.hpp"
#include "resident/domains/social/social_relation_execution.hpp"
namespace hanmo::resident {namespace {
constexpr const char* knowledge_key="social_knowledge_common_cut";
constexpr const char* original_key="social_original_relations";
constexpr const char* propagation_key="social_propagation_adoption_cut";
struct Selection{bool knowledge=false,original=false,propagation=false;};
bool exact_social(const PreparedCatalog& c){return c.exact_time&&(exact_social_selected(*c.exact_time)||exact_complete_selected(*c.exact_time));}
Outcome<Selection> selected(const BoundaryValue& scenario){try{
    using namespace boundary;const auto* dynamic=scenario.find("dynamic_world");if(!dynamic)return {true,{}, {}};
    const BoundaryValue* knowledge=nullptr;const BoundaryValue* original=nullptr;const BoundaryValue* propagation=nullptr;
    for(const auto& row:object(*dynamic,"scenario/dynamic_world")){
        const BoundaryValue** slot=nullptr;
        if(row.first==knowledge_key)slot=&knowledge;else if(row.first==original_key)slot=&original;else if(row.first==propagation_key)slot=&propagation;else continue;
        if(*slot)fail("DuplicateSocialSourceSelection",row.first);*slot=&row.second;
    }
    Selection out{bool(knowledge),bool(original),bool(propagation)};
    const auto version=[](const BoundaryValue*value,const char*key,Integer expected){if(!value)return;
        fields(*value,{"version"},{},key);if(object(*value,key).size()!=1||get_int(*value,"version",key)!=expected)unsupported("UnsupportedSocialSourceVersion",key);
    };
    version(knowledge,knowledge_key,1);version(original,original_key,2);version(propagation,propagation_key,1);
    if(!out.knowledge&&!out.original&&!out.propagation)return {true,out,{}};
    auto exact=decode_exact_time_profile(scenario);if(!exact.ok)return {false,{},std::move(exact.error)};
    if(!exact.value||!(exact_social_selected(*exact.value)||exact_complete_selected(*exact.value)))unsupported("SocialKnowledgeCutExactSourcesRequired",knowledge_key);
    if((out.original||out.propagation)&&!exact_complete_selected(*exact.value))unsupported("OriginalSocialCompleteSourcesRequired",original_key);
    if(out.propagation&&(!out.knowledge||!out.original))unsupported("SocialPropagationOriginalCutsRequired",propagation_key);
    return {true,out,{}};
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status append(CatalogIdentity& identity,const Selection& value){
    for(const auto& row:identity.module_versions)if(row.first==knowledge_key||row.first=="social"||row.first==propagation_key)return common_failure("DuplicateSocialSourceModule",row.first,ErrorKind::internal);
    if(value.knowledge)append_native_catalog_module(identity,knowledge_key,"1");
    if(value.original)append_native_catalog_module(identity,"social","2");
    if(value.propagation)append_native_catalog_module(identity,propagation_key,"1");
    return common_success();
}
Outcome<bool> module(const CatalogIdentity& identity,const char*key,const char*version){
    bool seen=false;for(const auto& row:identity.module_versions)if(row.first==key){
        if(seen||row.second!=version)return {false,false,common_failure("SocialSourceModuleMismatch",key,ErrorKind::internal).error};seen=true;
    }return {true,seen,{}};
}
Status state_version(const DynamicSocialState& state,Integer selected){
    if(state.version!=selected||bool(state.time2)!=social_time2_version(selected))return common_failure("DynamicSocialSelectedStateMismatch",{},ErrorKind::internal);
    return common_success();
}
}
Outcome<bool> selected_native_social_knowledge_cut(const CatalogIdentity& identity){return module(identity,knowledge_key,"1");}
Outcome<bool> selected_native_social_propagation_cut(const CatalogIdentity& identity){return module(identity,propagation_key,"1");}
Outcome<Integer> selected_native_original_social_version(const CatalogIdentity& identity){
    auto actual=module(identity,"social","2");if(!actual.ok)return {false,0,std::move(actual.error)};return {true,actual.value?2:1,{}};
}
Status append_native_social_source_modules(const BoundaryValue& scenario,CatalogIdentity& identity){
    auto selection=selected(scenario);if(!selection.ok)return {false,{},std::move(selection.error)};return append(identity,selection.value);
}
Status prepare_native_social_source_selection(const BoundaryValue& data,const BoundaryValue& scenario,PreparedCatalog& catalog,SocialCatalog& original){
    if(!catalog.social||catalog.social.get()!=&original)return common_failure("SocialCatalogCandidateMismatch",{},ErrorKind::internal);
    if(catalog.social_knowledge_common_cut_selected||original.original_relations)return common_failure("SocialSourcesAlreadyPrepared",{},ErrorKind::internal);
    auto selection=selected(scenario);if(!selection.ok)return {false,{},std::move(selection.error)};
    auto added=append(catalog.identity,selection.value);if(!added.ok)return added;
    catalog.social_knowledge_common_cut_selected=selection.value.knowledge;
    if(selection.value.original){
        // data is the very same effective/scenario-overridden author tree used
        // to initialize SocialState. No present relation is used as author data.
        auto locked=decode_original_social_relation_catalog(data,catalog.identity);if(!locked.ok)return {false,{},std::move(locked.error)};
        original.original_relations=std::move(locked.value);
    }
    return common_success();
}
Status validate_native_social_catalog_selection(const PreparedCatalog& catalog){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 155, "PreparedCatalog", "validate_native_social_catalog_selection", &catalog);

    auto cut=selected_native_social_knowledge_cut(catalog.identity);if(!cut.ok)return {false,{},std::move(cut.error)};
    auto original=selected_native_original_social_version(catalog.identity);if(!original.ok)return {false,{},std::move(original.error)};
    auto propagation=selected_native_social_propagation_cut(catalog.identity);if(!propagation.ok)return {false,{},std::move(propagation.error)};
    if(cut.value!=catalog.social_knowledge_common_cut_selected)return common_failure("SocialKnowledgeCutCatalogMismatch",{},ErrorKind::internal);
    bool period=false;for(const auto& row:catalog.identity.module_versions)if(row.first=="social_period_sources"){
        if(period||row.second!="1")return common_failure("SocialPeriodSourceModuleMismatch",{},ErrorKind::internal);period=true;
    }
    if(period!=exact_social(catalog))return common_failure("SocialExactCatalogModuleMismatch",{},ErrorKind::internal);
    if((cut.value||period||original.value==2||propagation.value)&&(catalog.dynamic_profile==DynamicProfile::disabled||!catalog.social||!catalog.common))
        return common_failure("SocialOriginalSourcesRequired",{},ErrorKind::unsupported_capability);
    if(cut.value&&!period)return common_failure("SocialKnowledgeCutExactSourcesRequired",{},ErrorKind::unsupported_capability);
    if(original.value==2&&(!period||!catalog.exact_time||!exact_complete_selected(*catalog.exact_time)))return common_failure("OriginalSocialCompleteSourcesRequired",{},ErrorKind::unsupported_capability);
    if(bool(catalog.social&&catalog.social->original_relations)!=(original.value==2))return common_failure("OriginalSocialCatalogSelectionMismatch",{},ErrorKind::internal);
    if(original.value==2&&!same_original_catalog_identity(catalog.social->original_relations->identity,catalog.identity))return common_failure("OriginalSocialLockedCatalogMismatch",{},ErrorKind::internal);
    if(propagation.value&&(!cut.value||original.value!=2))return common_failure("SocialPropagationOriginalCutsRequired",{},ErrorKind::unsupported_capability);
    return common_success();
}
Outcome<Integer> selected_native_dynamic_social_version(const PreparedCatalog& catalog){
    auto valid=validate_native_social_catalog_selection(catalog);if(!valid.ok)return {false,0,std::move(valid.error)};
    if(catalog.dynamic_profile==DynamicProfile::disabled)return {true,0,{}};
    const Integer original=catalog.regional_life?2:1;
    if(!exact_social(catalog))return {true,original,{}};
    auto propagation=selected_native_social_propagation_cut(catalog.identity);if(!propagation.ok)return {false,0,std::move(propagation.error)};
    return select_social_time2_physical_version(original,catalog.social_knowledge_common_cut_selected,propagation.value);
}
Status validate_native_social_state_selection(const SocialState& state,const PreparedCatalog& catalog){
    auto selected=selected_native_dynamic_social_version(catalog);if(!selected.ok)return {false,{},std::move(selected.error)};
    auto original=selected_native_original_social_version(catalog.identity);if(!original.ok)return {false,{},std::move(original.error)};
    auto main=validate_social_relation_state_selection(state,original.value);if(!main.ok)return main;
    if(bool(state.dynamic)!=bool(selected.value))return common_failure("DynamicSocialSelectedStateMismatch",{},ErrorKind::internal);
    return state.dynamic?state_version(*state.dynamic,selected.value):common_success();
}
Outcome<SocialState> decode_native_social_seed(const BoundaryValue& value,const PreparedCatalog& catalog){try{
    auto selected=selected_native_dynamic_social_version(catalog);if(!selected.ok)return {false,{},std::move(selected.error)};
    auto version=selected_native_original_social_version(catalog.identity);if(!version.ok)return {false,{},std::move(version.error)};
    const auto& original=boundary::field(value,"social","state");const auto* dynamic=original.find("dynamic");
    if(bool(dynamic)!=bool(selected.value))return {false,{},common_failure("DynamicSocialSelectedStateMismatch",{},ErrorKind::internal).error};
    if(dynamic&&boundary::get_int(*dynamic,"version","social/dynamic")!=selected.value)return {false,{},common_failure("DynamicSocialSelectedStateMismatch",{},ErrorKind::internal).error};
    ScopedCheckpointJsonDecode original_decode(CheckpointWorldJsonSection::social);
    auto parsed=decode_social_seed(value,version.value);if(!parsed.ok)return parsed;
    auto valid=validate_native_social_state_selection(parsed.value,catalog);if(!valid.ok)return {false,{},std::move(valid.error)};return parsed;
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Outcome<BoundaryValue> encode_native_social_state(const SocialState& state,const PreparedCatalog& catalog){try{
    auto valid=validate_native_social_state_selection(state,catalog);if(!valid.ok)return {false,{},std::move(valid.error)};
    auto version=selected_native_original_social_version(catalog.identity);if(!version.ok)return {false,{},std::move(version.error)};
    return {true,encode_social_state(state,version.value),{}};
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status visit_native_social_checkpoint(CodecVisitor& codec,SocialState& state,const PreparedCatalog& catalog){
    auto valid=validate_native_social_catalog_selection(catalog);if(!valid.ok)return valid;
    auto version=selected_native_original_social_version(catalog.identity);if(!version.ok)return {false,{},std::move(version.error)};
    // dynamic_social is a later independent section in binary restore.
    auto result=visit_social_checkpoint(codec,state,version.value);if(!result.ok)return result;
    return validate_social_relation_state_selection(state,version.value);
}
Status visit_native_dynamic_social_checkpoint(CodecVisitor& codec,DynamicSocialState& state,const PreparedCatalog& catalog){
    auto selected=selected_native_dynamic_social_version(catalog);if(!selected.ok)return {false,{},std::move(selected.error)};
    if(!selected.value)return common_failure("UnexpectedDynamicSocialCheckpoint",{},ErrorKind::internal);
    if(!codec.reading()){auto valid=state_version(state,selected.value);if(!valid.ok)return valid;}
    auto result=social_time2_version(selected.value)?visit_dynamic_social_time2_checkpoint(codec,state,selected.value):visit_dynamic_social_checkpoint(codec,state);
    if(!result.ok)return result;return state_version(state,selected.value);
}
Status emit_native_social_state_fields(ProjectionWriter& writer,const SocialState& state,const PreparedCatalog& catalog){
    auto valid=validate_native_social_state_selection(state,catalog);if(!valid.ok)return valid;
    auto version=selected_native_original_social_version(catalog.identity);if(!version.ok)return {false,{},std::move(version.error)};
    return emit_social_state_fields(writer,state,version.value);
}
}
