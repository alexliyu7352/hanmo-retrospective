#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "family_projection.hpp"
#include "family_source_projection.hpp"
#include "resident/domains/interaction/personal_dialogues/known_personal_dialogue_proof_codec.hpp"
#include "voluntary_path_state.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include <set>
namespace hanmo::resident {namespace {
using V=BoundaryValue;
bool reference(const StableId& id){return !id.empty()&&id.size()<=256;}
bool order(const FieldOrder& got,const std::set<StableId>& expected){return got.empty()||(got.size()==expected.size()&&std::set<StableId>(got.begin(),got.end())==expected);}
Status cancellation_shape(const VoluntaryPathCancellation& f){
    if(f.kind!=voluntary_path_cancelled_kind||!reference(f.plan_id)||!reference(f.actor_id)||f.tick<0||f.tick>2147483647||f.revision<0||
       (f.reason!="predecessor_cancel"&&f.reason!="successor_withdrawal")||bool(f.withdrawal)!=(f.reason=="successor_withdrawal"))return common_failure("InvalidVoluntaryPathCancellation");
    std::set<StableId> fields{"kind","plan_id","actor_id","reason","tick","revision"};if(f.withdrawal)fields.insert("withdrawal");
    if(!order(f.field_order,fields))return common_failure("BoundaryFieldOrderMismatch");
    if(f.withdrawal){VoluntaryPathSource s;s.statement=*f.withdrawal;auto valid=validate_voluntary_path_source_shape(s);if(!valid.ok)return valid;
        const auto& m=f.withdrawal->declaration.meeting;if(m.actor_id!=f.actor_id||m.source_identity.tick!=f.tick||m.source_identity.revision!=f.revision)
            return common_failure("VoluntaryPathWithdrawalSourceMismatch");}
    return common_success();
}
Status commit_shape(const VoluntaryPathCommitBinding& f){
    if(!reference(f.result_id)||!reference(f.actor_id)||!reference(f.kind)||f.request_seq<1||f.revision<1)return common_failure("InvalidVoluntaryPathCommit");
    return order(f.field_order,{"result_id","actor_id","kind","request_seq","revision"})?common_success():common_failure("BoundaryFieldOrderMismatch");
}
Status visit_cancellation(CodecVisitor& c,VoluntaryPathCancellation& f){c.field("kind",f.kind);c.field("plan_id",f.plan_id);c.field("actor_id",f.actor_id);c.field("reason",f.reason);c.field("tick",f.tick);c.field("revision",f.revision);c.optional("withdrawal",f.withdrawal,[](CodecVisitor& v,KnownPersonalDialogueProof& p){return visit_known_personal_dialogue_proof(v,p);});common_checkpoint::strings(c,"field_order",f.field_order);if(!c.status().ok)return c.status();auto valid=cancellation_shape(f);return valid.ok?c.status():c.reject(valid.error.original.code);}
Status visit_commit(CodecVisitor& c,VoluntaryPathCommitBinding& f){c.field("result_id",f.result_id);c.field("actor_id",f.actor_id);c.field("kind",f.kind);c.field("request_seq",f.request_seq);c.field("revision",f.revision);common_checkpoint::strings(c,"field_order",f.field_order);if(!c.status().ok)return c.status();auto valid=commit_shape(f);return valid.ok?c.status():c.reject(valid.error.original.code);}
}
Status validate_voluntary_path_state_shape(const VoluntaryPathState& state){
    if(!order(state.field_order,{"consents","cancellations","completion_revisions","commits"}))return common_failure("BoundaryFieldOrderMismatch");
    if(state.consents.size()>CodecVisitor::max_items||state.cancellations.size()>CodecVisitor::max_items||state.completion_revisions.size()>CodecVisitor::max_items||state.commits.size()>CodecVisitor::max_items)return common_failure("VoluntaryPathStateLimit");
    for(const auto& row:state.consents.rows()){if(!reference(row.first))return common_failure("InvalidVoluntaryPathSourceId");auto v=validate_voluntary_path_source_shape(row.second);if(!v.ok)return v;}
    for(const auto& row:state.cancellations.rows()){if(!reference(row.first))return common_failure("InvalidVoluntaryPathSourceId");auto v=cancellation_shape(row.second);if(!v.ok)return v;}
    for(const auto& row:state.completion_revisions.rows())if(!reference(row.first)||row.second<0)return common_failure("InvalidVoluntaryPathCompletionRevision");
    for(const auto& row:state.commits.rows()){if(!reference(row.first))return common_failure("InvalidVoluntaryPathSourceId");auto v=commit_shape(row.second);if(!v.ok)return v;}
    return common_success();
}
Outcome<VoluntaryPathState> decode_voluntary_path_state(const V& value){try{
    auto unique=validate_succession_json_unique(value);if(!unique.ok)return {false,{},unique.error};using namespace boundary;
    fields(value,{"consents","cancellations","completion_revisions","commits"},{},"family_voluntary_paths");VoluntaryPathState out;
    for(const auto& row:object(field(value,"consents",""),"consents")){auto p=decode_voluntary_path_source(row.second);if(!p.ok)return {false,{},p.error};if(!out.consents.insert(row.first,std::move(p.value)))fail("DuplicateVoluntaryPathConsentSource",row.first);}
    for(const auto& row:object(field(value,"cancellations",""),"cancellations")){
        const auto& v=row.second;fields(v,{"kind","plan_id","actor_id","reason","tick","revision"},{"withdrawal"},row.first);
        VoluntaryPathCancellation f;f.kind=get_string(v,"kind","");f.plan_id=get_string(v,"plan_id","");f.actor_id=get_string(v,"actor_id","");f.reason=get_string(v,"reason","");f.tick=get_int(v,"tick","");f.revision=get_int(v,"revision","");
        if(const auto* p=v.find("withdrawal")){auto proof=decode_known_personal_dialogue_proof(*p);if(!proof.ok)return {false,{},proof.error};f.withdrawal=std::move(proof.value);}f.field_order=order(v);
        if(!out.cancellations.insert(row.first,std::move(f)))fail("DuplicateVoluntaryPathCancellation",row.first);
    }
    for(const auto& row:object(field(value,"completion_revisions",""),"completion_revisions"))if(!out.completion_revisions.insert(row.first,integer(row.second,row.first)))fail("DuplicateVoluntaryPathCompletionSource",row.first);
    for(const auto& row:object(field(value,"commits",""),"commits")){
        const auto& v=row.second;fields(v,{"result_id","actor_id","kind","request_seq","revision"},{},row.first);
        VoluntaryPathCommitBinding f;f.result_id=get_string(v,"result_id","");f.actor_id=get_string(v,"actor_id","");f.kind=get_string(v,"kind","");f.request_seq=get_int(v,"request_seq","");f.revision=get_int(v,"revision","");f.field_order=order(v);
        if(!out.commits.insert(row.first,std::move(f)))fail("DuplicateVoluntaryPathCommit",row.first);
    }
    out.field_order=order(value);auto valid=validate_voluntary_path_state_shape(out);return valid.ok?Outcome<VoluntaryPathState>{true,std::move(out),{}}:Outcome<VoluntaryPathState>{false,{},valid.error};
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
Status emit_voluntary_path_cancellation_fields(ProjectionWriter& w,const VoluntaryPathCancellation& f){w.field("kind",f.kind);w.field("plan_id",f.plan_id);w.field("actor_id",f.actor_id);w.field("reason",f.reason);w.field("tick",f.tick);w.field("revision",f.revision);if(f.withdrawal)w.object("withdrawal",[&](auto& out){return emit_known_personal_dialogue_proof_fields(out,*f.withdrawal);});return w.order_fields(f.field_order);}
Status emit_voluntary_path_commit_fields(ProjectionWriter& w,const VoluntaryPathCommitBinding& f){w.field("result_id",f.result_id);w.field("actor_id",f.actor_id);w.field("kind",f.kind);w.field("request_seq",f.request_seq);w.field("revision",f.revision);return w.order_fields(f.field_order);}
Status emit_voluntary_path_state_fields(ProjectionWriter& w,const VoluntaryPathState& s){auto valid=validate_voluntary_path_state_shape(s);if(!valid.ok)return w.reject(valid.error.original.code,voluntary_path_module);w.object("consents",[&](auto& table){for(const auto& row:s.consents.rows())table.object(row.first,[&](auto& record){return emit_voluntary_path_source_fields(record,row.second);});return table.status();});w.object("cancellations",[&](auto& table){for(const auto& row:s.cancellations.rows())table.object(row.first,[&](auto& record){return emit_voluntary_path_cancellation_fields(record,row.second);});return table.status();});w.object("completion_revisions",[&](auto& table){for(const auto& row:s.completion_revisions.rows())table.field(row.first,row.second);return table.status();});w.object("commits",[&](auto& table){for(const auto& row:s.commits.rows())table.object(row.first,[&](auto& record){return emit_voluntary_path_commit_fields(record,row.second);});return table.status();});return w.order_fields(s.field_order);}
BoundaryValue encode_voluntary_path_state(const VoluntaryPathState& s){return family_projection::encode([&](auto& w){return emit_voluntary_path_state_fields(w,s);});}
Status visit_voluntary_path_checkpoint(CodecVisitor& c,VoluntaryPathState& s){
    if(!c.reading()){auto valid=validate_voluntary_path_state_shape(s);if(!valid.ok)return c.reject(valid.error.original.code);}
    common_checkpoint::table(c,"consents",s.consents,[](CodecVisitor& v,VoluntaryPathSource& p){return visit_voluntary_path_source(v,p);});
    common_checkpoint::table(c,"cancellations",s.cancellations,visit_cancellation);common_checkpoint::table(c,"completion_revisions",s.completion_revisions,[](CodecVisitor& v,Integer& n){return v.field("value",n);});common_checkpoint::table(c,"commits",s.commits,visit_commit);common_checkpoint::strings(c,"field_order",s.field_order);
    if(!c.status().ok)return c.status();auto valid=validate_voluntary_path_state_shape(s);return valid.ok?c.status():c.reject(valid.error.original.code);
}
}
