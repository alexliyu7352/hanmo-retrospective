#include "obligation_ref.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/projection_writer.hpp"
#include <array>
namespace hanmo::resident {namespace {
struct ProjectFields {ProjectionWriter&w;void operator()(const char*key,const StableId&value){w.field(key,value);}};
struct VisitFields {CodecVisitor&c;void operator()(const char*key,StableId&value){c.field(key,value);}};
struct ReadFields {
 const BoundaryValue&input;std::array<const char*,2> fields{};std::size_t count=0;
 void operator()(const char*key,StableId&value) {
  if(count==fields.size())boundary::fail("ObligationReferenceFieldLimit","/obligation_ref");
  fields[count++]=key;value=boundary::string(boundary::field(input,key,"/obligation_ref"),StableId("/obligation_ref/")+key);
 }
 void finish()const {
  for(const auto&row:boundary::object(input,"/obligation_ref")) {
   bool found=false;for(std::size_t i=0;i<count;++i)found|=row.first==fields[i];
   if(!found)boundary::fail("UnsupportedObligationReferenceField",StableId("/obligation_ref/")+row.first);
  }
 }
};
}
Status validate_obligation_ref_shape(const ObligationRef&ref) {
 if(ref.obligation_id.empty())return common_failure("ObligationOriginalIdRequired");
 if(ref.owner_domain!="local_economy"&&ref.owner_domain!="tasks"&&ref.owner_domain!="commerce")return common_failure("ObligationOwnerUnsupported",ref.owner_domain);
 return common_success();
}
bool same_obligation_ref(const ObligationRef&a,const ObligationRef&b) {return a.owner_domain==b.owner_domain&&a.obligation_id==b.obligation_id;}
Status visit_obligation_ref(CodecVisitor&c,ObligationRef&ref) {
 if(!c.reading()){auto valid=validate_obligation_ref_shape(ref);if(!valid.ok)return c.reject(valid.error.original.code);}
 VisitFields fields{c};obligation_ref_fields(fields,ref);
 if(c.status().ok){auto valid=validate_obligation_ref_shape(ref);if(!valid.ok)return c.reject(valid.error.original.code);}
 return c.status();
}
Status emit_obligation_ref(ProjectionWriter&w,const StableId&name,const ObligationRef&ref) {
 auto valid=validate_obligation_ref_shape(ref);if(!valid.ok)return w.reject(valid.error.original.code,name);
 return w.object(name,[&](auto&out){ProjectFields fields{out};obligation_ref_fields(fields,ref);return out.status();});
}
Outcome<BoundaryValue> encode_obligation_ref(const ObligationRef&ref) {
 BoundaryValue out;BoundaryProjectionWriter writer(out);auto result=emit_obligation_ref(writer,"",ref);if(result.ok)result=writer.finish();
 if(!result.ok)return {false,{},result.error};return {true,std::move(out),{}};
}
Outcome<ObligationRef> decode_obligation_ref(const BoundaryValue&input) {try {
 ObligationRef ref;ReadFields fields{input};obligation_ref_fields(fields,ref);fields.finish();auto valid=validate_obligation_ref_shape(ref);
 if(!valid.ok)return {false,{},valid.error};return {true,std::move(ref),{}};
 }catch(const boundary::DecodeFailure&e){return {false,{},e.error};}}
}
