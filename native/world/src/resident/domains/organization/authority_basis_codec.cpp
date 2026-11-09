#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
// AuthorityBasis的单一完整字段序；冷持久值不含candidate/read_versions或活指针。
#include "authority_basis_codec_fields.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
namespace hanmo::resident {
using namespace authority_basis_codec_fields;

Status visit_authority_basis(CodecVisitor& c,AuthorityBasis& value){return basis(c,value);}
Status emit_authority_basis_projection(ProjectionWriter& sink,std::string_view name,const AuthorityBasis& input){return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,input,[&](auto& c,auto& value){return basis(c,value);});}
Outcome<BoundaryValue> encode_authority_basis(const AuthorityBasis& input){BoundaryValue out;BoundaryProjectionWriter sink(out);auto valid=emit_authority_basis_projection(sink,"",input);if(valid.ok)valid=sink.finish();return {valid.ok,std::move(out),valid.error};}
Outcome<AuthorityBasis> decode_authority_basis(const BoundaryValue& input){AuthorityBasis value;ActorFoundationJsonVisitor c(input);auto valid=basis(c,value);if(valid.ok)valid=c.finish();return {valid.ok,std::move(value),valid.error};}
}
