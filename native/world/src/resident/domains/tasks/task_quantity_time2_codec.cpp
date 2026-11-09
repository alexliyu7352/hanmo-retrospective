#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "task_quantity_time2_codec_fields.hpp"
#include "task_dynamic_projection.hpp"
namespace hanmo::resident {
Outcome<TaskRecord> decode_exact_quantity_task_record(const BoundaryValue&value){TaskRecord record;dynamic_codec::JsonArchive archive(value);auto s=task_quantity_time2_codec::record(archive,record);if(s.ok)s=archive.finish();return s.ok?Outcome<TaskRecord>{true,std::move(record),{}}:Outcome<TaskRecord>{false,{},s.error};}
Outcome<BoundaryValue> encode_exact_quantity_task_record(const TaskRecord&value){return dynamic_projection::encode(value,[](auto&a,auto&v){return task_quantity_time2_codec::record(a,v);});}
Status visit_exact_quantity_task_record(CodecVisitor&a,TaskRecord&value){return task_quantity_time2_codec::record(a,value);}
Status emit_exact_quantity_task_record_fields(ProjectionWriter&w,const TaskRecord&value){DynamicProjectionArchive a(w);return task_quantity_time2_codec::record(a,const_cast<TaskRecord&>(value));}
Outcome<DeliveryFact> decode_task_quantity_delivery_source2(const BoundaryValue&value){DeliveryFact result;dynamic_codec::JsonArchive a(value);auto s=task_quantity_time2_codec::delivery_source2(a,result);if(s.ok)s=a.finish();return s.ok?Outcome<DeliveryFact>{true,std::move(result),{}}:Outcome<DeliveryFact>{false,{},s.error};}
Outcome<BoundaryValue> encode_task_quantity_delivery_source2(const DeliveryFact&value){return dynamic_projection::encode(value,[](auto&a,auto&v){return task_quantity_time2_codec::delivery_source2(a,v);});}
Status visit_task_quantity_delivery_source2(CodecVisitor&a,DeliveryFact&value){return task_quantity_time2_codec::delivery_source2(a,value);}
Status emit_task_quantity_delivery_source2_fields(ProjectionWriter&w,const DeliveryFact&value){DynamicProjectionArchive a(w);return task_quantity_time2_codec::delivery_source2(a,const_cast<DeliveryFact&>(value));}
Outcome<TaskTransferFact> decode_task_quantity_transfer_source2(const BoundaryValue&value){TaskTransferFact result;dynamic_codec::JsonArchive a(value);auto s=task_quantity_time2_codec::transfer_source2(a,result);if(s.ok)s=a.finish();return s.ok?Outcome<TaskTransferFact>{true,std::move(result),{}}:Outcome<TaskTransferFact>{false,{},s.error};}
Outcome<BoundaryValue> encode_task_quantity_transfer_source2(const TaskTransferFact&value){return dynamic_projection::encode(value,[](auto&a,auto&v){return task_quantity_time2_codec::transfer_source2(a,v);});}
Status visit_task_quantity_transfer_source2(CodecVisitor&a,TaskTransferFact&value){return task_quantity_time2_codec::transfer_source2(a,value);}
Status emit_task_quantity_transfer_source2_fields(ProjectionWriter&w,const TaskTransferFact&value){DynamicProjectionArchive a(w);return task_quantity_time2_codec::transfer_source2(a,const_cast<TaskTransferFact&>(value));}

}
