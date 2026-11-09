// 主体后继沿原 Director 持值；作者、目录和所有冷出口共用同一有限选择。
#pragma once
#include "hanmo/resident/model_contract.hpp"
#include "resident/domains/director/director_model.hpp"
#include <string_view>
namespace hanmo::resident {
struct NativeDirectorSubjectSelection {
    Integer physical_version=0;
    DirectorSubjectsProfile2 subjects;
};
// 显式块不提供默认版本或新玩法参数；旧作者缺省仍走原 1..4。
Outcome<NativeDirectorSubjectSelection> decode_native_director_subject_selection(const BoundaryValue& scenario);
Outcome<Integer> selected_native_director_version(const CatalogIdentity&);
Status validate_native_director_catalog_selection(const PreparedCatalog&);
Outcome<DirectorState> decode_native_director_state(const BoundaryValue&,const PreparedCatalog&);
Outcome<BoundaryValue> encode_native_director_state(const DirectorState&,const PreparedCatalog&);
Status visit_native_director_checkpoint(CodecVisitor&,DirectorState&,const PreparedCatalog&);
class ProjectionWriter;
Status emit_native_director_projection(ProjectionWriter&,std::string_view,const DirectorState&,const PreparedCatalog&);
}
