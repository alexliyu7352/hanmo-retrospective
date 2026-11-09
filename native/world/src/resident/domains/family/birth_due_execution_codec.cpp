#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "birth_due_execution_codec_fields.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
namespace hanmo::resident {
Status visit_birth_due_execution(CodecVisitor& c,BirthDueExecution& r){return birth_due_codec::execution(c,r);}
Status emit_birth_due_execution_fields(ProjectionWriter& w,const BirthDueExecution& r){ActorFoundationProjectionArchive c(w);auto& borrowed=const_cast<BirthDueExecution&>(r);return birth_due_codec::execution(c,borrowed);}
Outcome<BirthDueExecution> decode_birth_due_execution(const BoundaryValue& v){BirthDueExecution out;ActorFoundationJsonVisitor c(v);auto valid=birth_due_codec::execution(c,out);if(valid.ok)valid=c.finish();return {valid.ok,std::move(out),valid.error};}
}
