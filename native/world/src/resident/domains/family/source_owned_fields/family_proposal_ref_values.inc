// 六个原FamilyProposalRef2分支的唯一拥有成员；不带原业务表或许可。
#pragma once
// FamilyMarriageProposalRef2 的实际拥有成员同原clone/retention共用。
template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyMarriageProposalRef2 &value,
                  Visitor &&visitor) {
  visitor(value.id);
}

// FamilyBirthProposalRef2 的实际拥有成员同原clone/retention共用。
template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyBirthProposalRef2 &value,
                  Visitor &&visitor) {
  visitor(value.id);
}

// FamilyAdultAdoptionProposalRef2 的实际拥有成员同原clone/retention共用。
template <class Visitor>
inline void
visit(const ::hanmo::resident::FamilyAdultAdoptionProposalRef2 &value,
      Visitor &&visitor) {
  visitor(value.id);
}

// FamilyCollateralProposalRef2 的实际拥有成员同原clone/retention共用。
template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyCollateralProposalRef2 &value,
                  Visitor &&visitor) {
  visitor(value.id);
}

// FamilyApprenticeshipProposalRef2 的实际拥有成员同原clone/retention共用。
template <class Visitor>
inline void
visit(const ::hanmo::resident::FamilyApprenticeshipProposalRef2 &value,
      Visitor &&visitor) {
  visitor(value.id);
}

// FamilyEducationProposalRef2 的实际拥有成员同原clone/retention共用。
template <class Visitor>
inline void visit(const ::hanmo::resident::FamilyEducationProposalRef2 &value,
                  Visitor &&visitor) {
  visitor(value.id);
  visitor(value.terms_version);
}

