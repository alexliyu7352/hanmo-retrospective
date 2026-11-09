#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "private_shop_capability_fields.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
namespace hanmo::resident {
Outcome<PrivateShopOperationCapabilitiesState> decode_private_shop_operation_capabilities(const BoundaryValue&v){
 return exact_fields::guarded<PrivateShopOperationCapabilitiesState>([&]{PrivateShopOperationCapabilitiesState out;ActorFoundationJsonVisitor c(v);exact_fields::take(private_shop_capability_fields::state(c,out));exact_fields::take(c.finish());return out;});
}
Status emit_private_shop_operation_capabilities_fields(ProjectionWriter&w,const PrivateShopOperationCapabilitiesState&v){
 try{ActorFoundationProjectionArchive c(w);return private_shop_capability_fields::state(c,const_cast<PrivateShopOperationCapabilitiesState&>(v));}catch(const exact_fields::CodecFailure&e){return {false,{},e.error};}
}
Status emit_private_shop_operation_capabilities(ProjectionWriter&w,const StableId&name,const PrivateShopOperationCapabilitiesState&v){return w.object(name,[&](auto&out){return emit_private_shop_operation_capabilities_fields(out,v);});}
Outcome<BoundaryValue> encode_private_shop_operation_capabilities(const PrivateShopOperationCapabilitiesState&v){BoundaryValue out;BoundaryProjectionWriter w(out);auto status=emit_private_shop_operation_capabilities(w,"",v);if(status.ok)status=w.finish();return status.ok?Outcome<BoundaryValue>{true,std::move(out),{}}:Outcome<BoundaryValue>{false,{},status.error};}
Status visit_private_shop_operation_capabilities(CodecVisitor&c,PrivateShopOperationCapabilitiesState&v){return exact_fields::visit_binary(c,v,[](auto&out,auto&row){return private_shop_capability_fields::state(out,row);});}
}
