// Borrow the original two card roots when composing the S4 five-root section.
#pragma once
#include "card_acquisition_contract.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
namespace hanmo::resident {
class ActorFoundationJsonVisitor;
Status visit_profile_acquisition_source(CodecVisitor&,ProfileAcquisitionSource&);
Status visit_profile_acquisition_source(ActorFoundationJsonVisitor&,ProfileAcquisitionSource&);
Status visit_profile_acquisition_source(ActorFoundationProjectionArchive&,ProfileAcquisitionSource&);
Status visit_card_acquisition_fields(CodecVisitor&,CardAcquisitionState&);
Status visit_card_acquisition_fields(ActorFoundationJsonVisitor&,CardAcquisitionState&);
Status visit_card_acquisition_fields(ActorFoundationProjectionArchive&,CardAcquisitionState&);
}
