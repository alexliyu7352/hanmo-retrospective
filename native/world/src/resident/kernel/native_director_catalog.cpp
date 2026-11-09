#include "resident/kernel/catalog_section_observation.hpp"
#include "native_director_catalog.hpp"
#include "exact_time_profile.hpp"
#include "resident/domains/director/director_subject2.hpp"
#include "resident/domains/director/director_projection.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
Outcome<NativeDirectorSubjectSelection> decode_native_director_subject_selection(const BoundaryValue& scenario){try{
    using namespace boundary;
    const auto* dynamic=scenario.find("dynamic_world");if(!dynamic)return {true,{},{}};
    const BoundaryValue* selected=nullptr;
    for(const auto& row:object(*dynamic,"scenario/dynamic_world"))if(row.first=="director_subjects"){
        if(selected)fail("DuplicateDirectorSubjectsSelection",row.first);selected=&row.second;
    }
    if(!selected)return {true,{},{}};
    fields(*selected,{"version","physical_version","selected_template_ids"},{},"director_subjects");
    if(object(*selected,"director_subjects").size()!=3||get_int(*selected,"version","director_subjects")!=2)
        fail("UnsupportedDirectorSubjectsVersion","director_subjects/version");
    NativeDirectorSubjectSelection result;result.physical_version=get_int(*selected,"physical_version","director_subjects");
    if(result.physical_version!=5&&result.physical_version!=6)fail("UnsupportedDirectorPhysicalVersion","director_subjects/physical_version");
    const auto& original=field(*dynamic,"director","dynamic_world");
    if(!original.find("registry_version")||get_int(original,"registry_version","director")!=1)
        unsupported("DirectorSubjectsOriginalRegistryRequired","director_subjects");
    auto exact=decode_exact_time_profile(scenario);if(!exact.ok)return {false,{},std::move(exact.error)};
    if(!exact.value||!exact_complete_selected(*exact.value))unsupported("DirectorSubjectsCompleteProfileRequired","director_subjects");
    result.subjects.enabled=true;
    result.subjects.selected_template_ids=strings(field(*selected,"selected_template_ids","director_subjects"),"director_subjects/selected_template_ids");
    // 真实领域 decoder 随后核原 18 个有限模板及各准入规则；这里不宣称 handler 已齐。
    if(result.subjects.selected_template_ids.size()>18)fail("DirectorSubjectTemplateLimit","director_subjects/selected_template_ids");
    return {true,std::move(result),{}};
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
Outcome<Integer> selected_native_director_version(const CatalogIdentity& identity){
    Integer physical=0;bool subjects=false,registry=false,complete=false;
    for(const auto& row:identity.module_versions){
        if(row.first=="director"){
            if(physical)return {false,{},common_failure("DuplicateDirectorModule",{},ErrorKind::internal).error};
            if(row.second=="1")physical=1;else if(row.second=="2")physical=2;else if(row.second=="3")physical=3;
            else if(row.second=="4")physical=4;else if(row.second=="5")physical=5;else if(row.second=="6")physical=6;
            else return {false,{},common_failure("UnsupportedDirectorPhysicalVersion",row.second,ErrorKind::internal).error};
        }else if(row.first=="director_subjects"){
            if(subjects||row.second!="2")return {false,{},common_failure("DirectorSubjectsModuleMismatch",{},ErrorKind::internal).error};subjects=true;
        }else if(row.first=="director_registry"){
            if(registry||row.second!="1")return {false,{},common_failure("DirectorRegistryModuleMismatch",{},ErrorKind::internal).error};registry=true;
        }else if(row.first=="actor_start_context"&&row.second=="3")complete=true;
    }
    if(subjects!=(physical==5||physical==6)||(subjects&&(!registry||!complete)))
        return {false,{},common_failure("DirectorSubjectsModuleMismatch",{},ErrorKind::internal).error};
    return {true,physical,{}};
}
Status validate_native_director_catalog_selection(const PreparedCatalog& catalog){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 130, "PreparedCatalog", "validate_native_director_catalog_selection", &catalog);

    auto selected=selected_native_director_version(catalog.identity);if(!selected.ok)return {false,{},std::move(selected.error)};
    if(bool(selected.value)!=bool(catalog.director))return common_failure("DirectorSelectedCatalogMismatch",{},ErrorKind::internal);
    if(!catalog.director)return common_success();
    const auto& original=*catalog.director;const bool subjects=selected.value==5||selected.value==6;
    if(original.subjects2!=subjects||original.command_acceptance_bindings!=(selected.value==6)||
       (subjects&&!original.dynamic_subjects))return common_failure("DirectorSelectedCatalogMismatch",{},ErrorKind::internal);
    if(subjects&&(!catalog.exact_time||!exact_complete_selected(*catalog.exact_time)))
        return common_failure("DirectorSubjectsCompleteProfileRequired",{},ErrorKind::unsupported_capability);
    const Integer legacy=original.dynamic_subjects?4:catalog.named_membership?3:catalog.regional_life?2:1;
    if(!subjects&&selected.value!=legacy)return common_failure("DirectorSelectedCatalogMismatch",{},ErrorKind::internal);
    return common_success();
}
Outcome<DirectorState> decode_native_director_state(const BoundaryValue& value,const PreparedCatalog& catalog){
    auto valid=validate_native_director_catalog_selection(catalog);if(!valid.ok)return {false,{},std::move(valid.error)};
    if(!catalog.director)return {false,{},common_failure("DirectorSelectedCatalogRequired",{},ErrorKind::internal).error};
    if(catalog.director->command_acceptance_bindings)return decode_director_state6(value,catalog.regional_life,catalog.named_membership);
    if(catalog.director->subjects2)return decode_director_state2(value,catalog.regional_life,catalog.named_membership);
    return decode_director_seed(value,catalog.regional_life,catalog.named_membership,catalog.director->dynamic_subjects);
}
Outcome<BoundaryValue> encode_native_director_state(const DirectorState& state,const PreparedCatalog& catalog){
    auto valid=validate_native_director_catalog_selection(catalog);if(!valid.ok)return {false,{},std::move(valid.error)};
    if(!catalog.director)return {false,{},common_failure("DirectorSelectedCatalogRequired",{},ErrorKind::internal).error};
    if(catalog.director->command_acceptance_bindings)return encode_director_state6(state);
    if(catalog.director->subjects2)return encode_director_state2(state);
    if(state.subject2)return {false,{},common_failure("UnexpectedDirectorSubjectState",{},ErrorKind::internal).error};
    return {true,encode_director_state(state),{}};
}
Status visit_native_director_checkpoint(CodecVisitor& codec,DirectorState& state,const PreparedCatalog& catalog){
    auto valid=validate_native_director_catalog_selection(catalog);if(!valid.ok)return valid;
    if(!catalog.director)return common_failure("DirectorSelectedCatalogRequired",{},ErrorKind::internal);
    if(catalog.director->command_acceptance_bindings)return visit_director_checkpoint6(codec,state,catalog.regional_life,catalog.named_membership);
    if(catalog.director->subjects2)return visit_director_checkpoint2(codec,state,catalog.regional_life,catalog.named_membership);
    if(!codec.reading()&&state.subject2)return common_failure("UnexpectedDirectorSubjectState",{},ErrorKind::internal);
    return visit_director_checkpoint(codec,state,catalog.regional_life,catalog.named_membership,catalog.director->dynamic_subjects);
}
Status emit_native_director_projection(ProjectionWriter& writer,std::string_view name,const DirectorState& state,const PreparedCatalog& catalog){
    auto valid=validate_native_director_catalog_selection(catalog);if(!valid.ok)return valid;
    if(!catalog.director)return common_failure("DirectorSelectedCatalogRequired",{},ErrorKind::internal);
    if(catalog.director->command_acceptance_bindings)return emit_director_state6_projection(writer,name,state);
    if(catalog.director->subjects2)return emit_director_state2_projection(writer,name,state);
    if(state.subject2)return common_failure("UnexpectedDirectorSubjectState",{},ErrorKind::internal);
    return writer.object(name,[&](auto& out){return emit_director_state_fields(out,state);});
}
}
