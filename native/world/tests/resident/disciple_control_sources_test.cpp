// 显式弟子政策仅选择原本人控制请求；此门不生成关系、同意或控制激活。
#include "resident/domains/family/disciple_control_sources.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {int checks=0;void check(bool yes,const char* message){++checks;if(!yes)throw std::runtime_error(message);}}
int main(){try{
 using V=BoundaryValue;using namespace boundary;
 auto input=obj({{"version",V{Integer{1}}},{"profile",V{"accepted_disciple_posthumous_control_v1"}},{"requests",obj({{"request",obj({{"request_id",V{"request"}},{"skill_id",V{"rites"}}})}})}});
 auto decoded=decode_disciple_control_catalog(input);check(decoded.ok&&decoded.value.enabled,"explicit exact original request selector decoded");
 LegacyControlConsentCatalog originals;originals.enabled=true;LegacyControlConsentDefinition original;original.id="request";original.predecessor_actor_id="teacher";original.successor_actor_id="student";original.dialogue_id="dialog";original.accept_node_id="consent";original.revoke_node_id="withdraw";originals.definitions.insert(original.id,original);CommonCatalog common;
 check(validate_disciple_control_catalog(decoded.value,originals,common).ok,"references original two-party control request");
 auto bad=decoded.value;bad.requests.find("request")->skill_id="naval";check(!validate_disciple_control_catalog(bad,originals,common).ok,"unselected extended skill rejected");common.extended_skills=true;check(validate_disciple_control_catalog(bad,originals,common).ok,"original sixteen skills profile permits naval relation");
 originals.definitions.erase("request");check(!validate_disciple_control_catalog(decoded.value,originals,common).ok,"missing original explicit consent request rejected");
 auto duplicate=input;duplicate.as<V::Dict>()->emplace_back("version",V{Integer{1}});check(!decode_disciple_control_catalog(duplicate).ok,"duplicate root selector rejected");auto extra=input;extra.as<V::Dict>()->emplace_back("accept_assets",V{true});check(!decode_disciple_control_catalog(extra).ok,"scope cannot grow into inheritance");
 DiscipleControlCatalog disabled;disabled.requests=decoded.value.requests;check(!validate_disciple_control_catalog(disabled,originals,common).ok,"no implicit activation from nonempty catalog");
 std::cout<<"DISCIPLE_CONTROL_SOURCES "<<checks<<" checks 0 failures\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
