#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "professional_order_codec_fields.hpp"
#include "resident/domains/tasks/task_dynamic_projection.hpp"
namespace hanmo::resident {
Outcome<OrderedProfessionalBinding> decode_ordered_professional_binding(const BoundaryValue&value){OrderedProfessionalBinding out;dynamic_codec::JsonArchive a(value);auto status=professional_order_codec::binding(a,out);if(status.ok)status=a.finish();return status.ok?Outcome<OrderedProfessionalBinding>{true,std::move(out),{}}:Outcome<OrderedProfessionalBinding>{false,{},status.error};}
Outcome<BoundaryValue> encode_ordered_professional_binding(const OrderedProfessionalBinding&v){return dynamic_projection::encode(v,[](auto&a,auto&r){return professional_order_codec::binding(a,r);});}
Status visit_ordered_professional_binding_checkpoint(CodecVisitor&a,OrderedProfessionalBinding&v){return professional_order_codec::binding(a,v);}
Status emit_ordered_professional_binding_fields(ProjectionWriter&w,const OrderedProfessionalBinding&v){DynamicProjectionArchive a(w);return professional_order_codec::binding(a,const_cast<OrderedProfessionalBinding&>(v));}
Outcome<OrderedProfessionalExecutionMember> decode_ordered_execution_member(const BoundaryValue&value){OrderedProfessionalExecutionMember out;dynamic_codec::JsonArchive a(value);auto status=professional_order_codec::member(a,out);if(status.ok)status=a.finish();return status.ok?Outcome<OrderedProfessionalExecutionMember>{true,std::move(out),{}}:Outcome<OrderedProfessionalExecutionMember>{false,{},status.error};}
Outcome<BoundaryValue> encode_ordered_execution_member(const OrderedProfessionalExecutionMember&v){return dynamic_projection::encode(v,[](auto&a,auto&r){return professional_order_codec::member(a,r);});}
Status visit_ordered_execution_member_checkpoint(CodecVisitor&a,OrderedProfessionalExecutionMember&v){return professional_order_codec::member(a,v);}
Status emit_ordered_execution_member_fields(ProjectionWriter&w,const OrderedProfessionalExecutionMember&v){DynamicProjectionArchive a(w);return professional_order_codec::member(a,const_cast<OrderedProfessionalExecutionMember&>(v));}
Outcome<OrderedCompletionFeeTerms> decode_ordered_completion_fee_terms(const BoundaryValue&value){OrderedCompletionFeeTerms out;dynamic_codec::JsonArchive a(value);auto status=professional_order_codec::fee_terms(a,out);if(status.ok)status=a.finish();return status.ok?Outcome<OrderedCompletionFeeTerms>{true,std::move(out),{}}:Outcome<OrderedCompletionFeeTerms>{false,{},status.error};}
Outcome<BoundaryValue> encode_ordered_completion_fee_terms(const OrderedCompletionFeeTerms&v){return dynamic_projection::encode(v,[](auto&a,auto&r){return professional_order_codec::fee_terms(a,r);});}
Status visit_ordered_completion_fee_terms_checkpoint(CodecVisitor&a,OrderedCompletionFeeTerms&v){return professional_order_codec::fee_terms(a,v);}
Status emit_ordered_completion_fee_terms_fields(ProjectionWriter&w,const OrderedCompletionFeeTerms&v){DynamicProjectionArchive a(w);return professional_order_codec::fee_terms(a,const_cast<OrderedCompletionFeeTerms&>(v));}
Outcome<OrderedCompletionChargeSource> decode_ordered_completion_charge(const BoundaryValue&value){OrderedCompletionChargeSource out;dynamic_codec::JsonArchive a(value);auto status=professional_order_codec::charge(a,out);if(status.ok)status=a.finish();return status.ok?Outcome<OrderedCompletionChargeSource>{true,std::move(out),{}}:Outcome<OrderedCompletionChargeSource>{false,{},status.error};}
Outcome<BoundaryValue> encode_ordered_completion_charge(const OrderedCompletionChargeSource&v){return dynamic_projection::encode(v,[](auto&a,auto&r){return professional_order_codec::charge(a,r);});}
Status visit_ordered_completion_charge_checkpoint(CodecVisitor&a,OrderedCompletionChargeSource&v){return professional_order_codec::charge(a,v);}
Status emit_ordered_completion_charge_fields(ProjectionWriter&w,const OrderedCompletionChargeSource&v){DynamicProjectionArchive a(w);return professional_order_codec::charge(a,const_cast<OrderedCompletionChargeSource&>(v));}
}
