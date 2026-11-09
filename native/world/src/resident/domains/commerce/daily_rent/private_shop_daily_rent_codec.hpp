// 新段独立严格 JSON/typed 编解码；旧 private_shops/1 的字段及 visitor 不变。
#pragma once
#include "private_shop_daily_rent.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
namespace hanmo::resident::dynamic_economy_codec {
template<class V> void record(V& v,PrivateShopDailyRentCatalog& x){v("version",x.version);v("profile",x.profile);v("arrears_policy",x.arrears_policy);}
template<class V> void record(V& v,PrivateShopRentAssessment& x){v("id",x.id);v("shop_id",x.shop_id);v("arrear_id",x.arrear_id);v("trigger",x.trigger);v("resume_fact_id",x.resume_fact_id);v("automatic_payment_fact_id",x.automatic_payment_fact_id);v("tick",x.tick);v("period_index",x.period_index);}
template<class V> void record(V& v,PrivateShopRentPayment& x){v("id",x.id);v("assessment_fact_id",x.assessment_fact_id);v("local_payment_id",x.local_payment_id);v("funding_kind",x.funding_kind);v("actor_id",x.actor_id);v("payer_wallet_id",x.payer_wallet_id);v("mandate_fact_id",x.mandate_fact_id);v("death_notice_binding_fact_id",x.death_notice_binding_fact_id);v("tick",x.tick);}
template<class V> void record(V& v,PrivateShopRentResumption& x){v("id",x.id);v("shop_id",x.shop_id);v("actor_id",x.actor_id);v("assessment_fact_id",x.assessment_fact_id);v("tick",x.tick);}
template<class V> void record(V& v,PrivateShopRentPlayerResult& x){v("id",x.id);v("actor_id",x.actor_id);v("command_kind",x.command_kind);v("result_id",x.result_id);v("request_seq",x.request_seq);v("revision",x.revision);v("tick",x.tick);}
template<class V> void record(V& v,PrivateShopDailyRentState& x){v("version",x.version);v("last_step_tick",x.last_step_tick);v("assessments",x.assessments);v("payments",x.payments);v("resumptions",x.resumptions);v("player_results",x.player_results);}
}
namespace hanmo::resident {
Outcome<PrivateShopDailyRentCatalog> decode_private_shop_daily_rent_catalog(const BoundaryValue&);
Outcome<std::optional<PrivateShopDailyRentCatalog>> decode_private_shop_daily_rent_profile(const BoundaryValue& scenario,const std::vector<StableId>& capabilities);
Outcome<PrivateShopDailyRentState> decode_private_shop_daily_rent_state(const BoundaryValue&);
class ProjectionWriter;
Status emit_private_shop_daily_rent_state_fields(ProjectionWriter&,const PrivateShopDailyRentState&);
BoundaryValue encode_private_shop_daily_rent_state(const PrivateShopDailyRentState&);
Status visit_private_shop_daily_rent_checkpoint(CodecVisitor&,PrivateShopDailyRentState&);
Status emit_private_shop_daily_rent_fact_payload_fields(ProjectionWriter&,const CommonStore&,const PrivateShopDailyRentState&,const FactRecord&);
Status validate_private_shop_daily_rent_fact_payload(ProjectionWriter&,const CommonStore&,const PrivateShopDailyRentState&,const FactRecord&);
Outcome<BoundaryValue> encode_private_shop_daily_rent_fact_payload(const CommonStore&,const PrivateShopDailyRentState&,const FactRecord&);
}
