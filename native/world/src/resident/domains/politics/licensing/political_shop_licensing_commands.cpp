// 申请人只选择原柜台和铺位；登记/解除/撤销另带本人公职，不能用申请命令代签行政处分。
#include "political_shop_licensing.hpp"
#include "resident/domains/politics/takeover/politics_takeover_payload_guard.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
namespace hanmo::resident::dynamic_economy_codec {
template<class A>void record(A& a,PoliticalRequestShopPermission& v){a("court_id",v.court_id);a("premises_id",v.premises_id);}
template<class A>void record(A& a,PoliticalRegisterWanted& v){a("authority",v.authority);a("court_id",v.court_id);a("actor_id",v.actor_id);a("reason_code",v.reason_code);}
template<class A>void record(A& a,PoliticalReleaseWanted& v){a("authority",v.authority);a("warrant_fact_id",v.warrant_fact_id);}
template<class A>void record(A& a,PoliticalRevokeShopPermit& v){a("authority",v.authority);a("permit_fact_id",v.permit_fact_id);}
}
namespace hanmo::resident {
StableId political_shop_licensing_command_kind(const PoliticalShopLicensingCommand& command){return std::holds_alternative<PoliticalRequestShopPermission>(command)?"political_request_shop_permission":std::holds_alternative<PoliticalRegisterWanted>(command)?"political_register_wanted":std::holds_alternative<PoliticalReleaseWanted>(command)?"political_release_wanted":"political_revoke_shop_permit";}
BoundaryValue encode_political_shop_licensing_command(const PoliticalShopLicensingCommand& command){return std::visit([](const auto& value){return dynamic_economy_codec::encode(value);},command);}
Outcome<PoliticalShopLicensingCommand> decode_political_shop_licensing_command(const StableId& kind,const BoundaryValue& value){auto guard=validate_political_takeover_payload(value);if(!guard.ok)return {false,{},guard.error};try{if(kind=="political_request_shop_permission"){PoliticalRequestShopPermission out;dynamic_economy_codec::decode(value,out,kind);return {true,std::move(out),{}};}if(kind=="political_register_wanted"){PoliticalRegisterWanted out;dynamic_economy_codec::decode(value,out,kind);return {true,std::move(out),{}};}if(kind=="political_release_wanted"){PoliticalReleaseWanted out;dynamic_economy_codec::decode(value,out,kind);return {true,std::move(out),{}};}if(kind=="political_revoke_shop_permit"){PoliticalRevokeShopPermit out;dynamic_economy_codec::decode(value,out,kind);return {true,std::move(out),{}};}return {false,{},common_failure("UnsupportedPoliticalShopLicensingCommand",kind).error};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
}
