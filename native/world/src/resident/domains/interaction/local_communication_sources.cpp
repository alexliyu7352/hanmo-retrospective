#include "local_communication_sources.hpp"
namespace hanmo::resident {
Outcome<const CommunicationSourceRead*>NativeLocalCommunicationRead::owner(const SourceRef&s)const{
 if(!face_.bound_to(common_,social_)||!custody_.bound_to(common_,social_)||(carrier_&&!carrier_->bound_to(common_,social_)))return {false,nullptr,common_failure("CommunicationOwnerCandidateMismatch",s.id).error};
 if(s.domain=="tasks"&&(s.kind=="task_report_carriage_dispatched"||s.kind=="task_report_handed_over")){if(!carrier_)return {false,nullptr,common_failure("OriginalCarrierCommunicationOwnerRequired",s.id,ErrorKind::unsupported_capability).error};return {true,carrier_,{}};}
 if(s.domain=="interaction"&&s.kind==face_to_face_report_source_kind)return {true,&face_,{}};
 if(s.domain=="visit_messages"&&(s.kind==visit_report_deposited_kind||s.kind==visit_report_picked_up_kind||s.kind==visit_report_withdrawn_kind))return {true,&custody_,{}};
 return {false,nullptr,common_failure("OriginalCommunicationOwnerRequired",s.id,ErrorKind::unsupported_capability).error};
}
Outcome<CommunicationOrigin>NativeLocalCommunicationRead::origin(const SourceRef&s,Integer schema)const{auto p=owner(s);return p.ok?p.value->origin(s,schema):Outcome<CommunicationOrigin>{false,{},p.error};}
Outcome<CommunicationCancellation>NativeLocalCommunicationRead::cancellation(const SourceRef&s,Integer schema)const{auto p=owner(s);return p.ok?p.value->cancellation(s,schema):Outcome<CommunicationCancellation>{false,{},p.error};}
Outcome<CommunicationArrival>NativeLocalCommunicationRead::arrival(const SourceRef&s,Integer schema)const{auto p=owner(s);return p.ok?p.value->arrival(s,schema):Outcome<CommunicationArrival>{false,{},p.error};}
Outcome<CommunicationResolvedRecipient>NativeLocalCommunicationRead::resolve_arrived_address(const CommunicationAddress&a,const CommunicationArrival&r)const{auto p=owner(r.source);return p.ok?p.value->resolve_arrived_address(a,r):Outcome<CommunicationResolvedRecipient>{false,{},p.error};}
Status NativeLocalCommunicationRead::validate_historical_recipient(const CommunicationAddress&a,const CommunicationArrival&r,const CommunicationResolvedRecipient&v)const{auto p=owner(r.source);return p.ok?p.value->validate_historical_recipient(a,r,v):Status{false,{},p.error};}
Outcome<CommunicationDossierAccess>NativeLocalCommunicationRead::dossier_access(const SourceRef&s,Integer schema)const{auto p=owner(s);return p.ok?p.value->dossier_access(s,schema):Outcome<CommunicationDossierAccess>{false,{},p.error};}
Status NativeLocalCommunicationRead::validate_delivery_before_common_source(const CommunicationDeliverySource&delivery,const SourceRef&later)const{auto p=owner(delivery.arrival_ref);return p.ok?p.value->validate_delivery_before_common_source(delivery,later):Status{false,{},p.error};}
Status NativeLocalCommunicationRead::may_disclose(const CommunicationOrigin&o,const Report&r)const{auto p=owner(o.source);return p.ok?p.value->may_disclose(o,r):Status{false,{},p.error};}
}
