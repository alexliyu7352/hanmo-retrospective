// 原M-TREATY源时间与有限结果投影均不反入Knowledge；完整许可/本人receipt由S6整体验源。
#pragma once
#include "treaty_recognition_sources.hpp"
#include "treaty_mandate_sources.hpp"
#include "treaty_release_sources.hpp"
namespace hanmo::resident {
struct TreatyProcedureMomentContext {
 const TreatyRevisionContext& revisions;
 const OrderedTable<TreatyPresentationSource>& presentations;
 const OrderedTable<TreatyReplyRecord>& replies;
 const OrderedTable<TreatySignatureSource>& signatures;
 const OrderedTable<TreatyActivationSource>& activations;
 const OrderedTable<TreatyTerminationSource>& terminations;
 const OrderedTable<TreatyMandateSource>& mandates;
};
bool native_treaty_procedure_source_kind(const FactRecord&);
Outcome<WorldMoment> read_treaty_procedure_source_moment(const TreatyProcedureMomentContext&,
 const MomentOwnerSource&);
class NativeTreatyProcedureResultRead final:public ExactResultOwnerRead {
 const TreatyProcedureMomentContext& context_;
public:explicit NativeTreatyProcedureResultRead(const TreatyProcedureMomentContext& x):context_(x){}
 Outcome<ExactOwnerFactRead> read_selected_fact(const FactRecord&)const override;
 Outcome<WorldMoment> read_selected_source_moment(const MomentOwnerSource&)const override;
};
}
