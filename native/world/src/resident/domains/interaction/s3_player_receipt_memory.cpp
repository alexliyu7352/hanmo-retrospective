#include "s3_player_receipt_memory.hpp"
#include "appointments/personal_appointment_state.hpp"
#include "contact_inquiries/contact_inquiry_state.hpp"
#include "visit_appointments/visit_appointment_state.hpp"
#include <type_traits>
#include <utility>
namespace hanmo::resident {namespace {
template<class Receipt>S3PlayerReceiptBuildView borrowed(const Receipt&v){
 return {v.result_id,v.request_actor_id,v.request_sequence,v.result_revision};
}
CheckpointCopyLease reserve(const S3PlayerReceiptBuildView&v){

 return CheckpointCopyLease::reserve();
}
}
// One original ownership operation, instantiated only for these existing
// nominal receipts. Their domain-specific proof and wire callbacks stay put.
#define HANMO_S3_PLAYER_RECEIPT_MEMORY(Type) \
Type::Type(const S3PlayerReceiptBuildView&v) \
 :copied_fields(reserve(v)),result_id(v.result_id),request_actor_id(v.request_actor_id), \
 request_sequence(v.request_sequence),result_revision(v.result_revision){} \
Type::Type(std::string_view result,std::string_view actor,Integer sequence,Integer revision) \
 :Type(S3PlayerReceiptBuildView{result,actor,sequence,revision}){} \
Type::Type(const Type&v):Type(borrowed(v)){} \
Type::Type(Type&&v):Type(borrowed(v)){} \
Type&Type::operator=(const Type&v){if(this!=&v){Type fresh(v);swap(fresh);}return *this;} \
Type&Type::operator=(Type&&v){if(this!=&v){Type fresh(std::move(v));swap(fresh);}return *this;} \
void Type::swap(Type&v)noexcept{ \
 static_assert(std::is_nothrow_swappable_v<CheckpointCopyLease>); \
 if(this==&v)return;using std::swap;swap(copied_fields,v.copied_fields); \
 result_id.swap(v.result_id);request_actor_id.swap(v.request_actor_id); \
 swap(request_sequence,v.request_sequence);swap(result_revision,v.result_revision); \
}
// A decoded receipt may only borrow its enclosing decode owner. Even its move
// therefore constructs a fresh independently charged target before escape.
HANMO_S3_PLAYER_RECEIPT_MEMORY(PersonalAppointmentPlayerReceipt)
HANMO_S3_PLAYER_RECEIPT_MEMORY(ContactInquiryPlayerReceipt)
HANMO_S3_PLAYER_RECEIPT_MEMORY(VisitAppointmentPlayerReceipt)
#undef HANMO_S3_PLAYER_RECEIPT_MEMORY
}
