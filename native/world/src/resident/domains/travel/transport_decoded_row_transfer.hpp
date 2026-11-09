// Explicit finite original row transfer used only inside a failing private
// restore candidate. CanonicalFraction's real copy may throw; it is not marked
// noexcept. All other aggregate members use their original move constructors.
#pragma once
#include "movement_record_codec_fields.hpp"
namespace hanmo::resident::transport_decoded_transfer {
template<class T>struct OriginalRow:std::false_type {};
#define K4_ORIGINAL_ROW(T) template<>struct OriginalRow<T>:std::true_type {}
K4_ORIGINAL_ROW(MovementBasis);
K4_ORIGINAL_ROW(SpatialWriterBinding);K4_ORIGINAL_ROW(MovementPrefixState);K4_ORIGINAL_ROW(MovementUnitState);
K4_ORIGINAL_ROW(TravelExertionBasis);K4_ORIGINAL_ROW(MovementExpenseSource);K4_ORIGINAL_ROW(MovementUnitCloseSource);
K4_ORIGINAL_ROW(TravelExertionSource);K4_ORIGINAL_ROW(MovementUnitBoundarySource);K4_ORIGINAL_ROW(MovementBasisTransitionSource);
K4_ORIGINAL_ROW(PersonalMovementControlSource);K4_ORIGINAL_ROW(MovementArrivalSource);K4_ORIGINAL_ROW(PersonalMovementTerminalSource);
K4_ORIGINAL_ROW(PatrolFinalMotionSource);K4_ORIGINAL_ROW(PatrolCheckpointArrival2);
K4_ORIGINAL_ROW(TerrainCreditDispositionSource);K4_ORIGINAL_ROW(ExactMovementContact);K4_ORIGINAL_ROW(MovementContactInstallationSource);
// CanonicalFraction 的真实转移可能抛出；失败撤整候选，不谎称 noexcept。
K4_ORIGINAL_ROW(MovementBoundaryContact);
// 原续行行含真实 WorldMoment；沿同失败撤候选的有限转移，不宣 noexcept。
K4_ORIGINAL_ROW(MovementBoundaryContinuationSource);
K4_ORIGINAL_ROW(MovementContactContinuationSource);K4_ORIGINAL_ROW(PassageModifierAdoption);
K4_ORIGINAL_ROW(CargoHandlingTerms);K4_ORIGINAL_ROW(CargoHandlingExecution);K4_ORIGINAL_ROW(CargoHandlingWorkResult);K4_ORIGINAL_ROW(CargoHandlingResult);
K4_ORIGINAL_ROW(GroupTransportExpenseSource);K4_ORIGINAL_ROW(TransportUnitState);K4_ORIGINAL_ROW(CarrierPosition);
K4_ORIGINAL_ROW(GroupMovementJourney);K4_ORIGINAL_ROW(GroupMovementBinding);K4_ORIGINAL_ROW(GroupJourneyStep);K4_ORIGINAL_ROW(GroupJourneyTerminal);
K4_ORIGINAL_ROW(JourneyPurposeBinding);K4_ORIGINAL_ROW(GroupMovementOpeningSource);K4_ORIGINAL_ROW(GroupMovementUnitTerms);K4_ORIGINAL_ROW(GroupMovementControlSource);K4_ORIGINAL_ROW(GroupMilitarySupplyRebindSource);
K4_ORIGINAL_ROW(InventorySpatialBinding);K4_ORIGINAL_ROW(TransportRegistrationSource);K4_ORIGINAL_ROW(TransportUnitReleaseSource);
K4_ORIGINAL_ROW(BoundaryCrossingReceipt);K4_ORIGINAL_ROW(TransportEntryManifestSource);K4_ORIGINAL_ROW(TransportPaymentReceipt);
#undef K4_ORIGINAL_ROW
template<class T>struct OriginalRow<std::pair<StableId,T>>:OriginalRow<T> {};
struct OriginalTransportDecodedRowTransfer {
 template<class T,class Allocator>Status operator()(std::vector<T,Allocator>& destination,T& source)const {
  static_assert(OriginalRow<T>::value,"an original transport row must explicitly own the fallible transfer");
  destination.emplace_back(std::move(source));return common_success();
 }
};
}
