// 必要坏档组件只核缓存与永久来源一一相符，不声称完整World死亡链已运行。
#include "resident/domains/family/family_legacy_receipt.hpp"
#include <iostream>
using namespace hanmo::resident;
int main(){int checks=0,failures=0;auto check=[&](bool value){++checks;if(!value)++failures;};FamilyLegacyState state;FamilyLegacyConsentFact consent;consent.source.predecessor_actor_id="former";consent.source.successor_actor_id="next";consent.source.tick=3;state.consents.insert("family_legacy:1",consent);FamilyLegacyRevocationFact revoked;revoked.source=consent.source;revoked.source.tick=4;state.revocations.insert("family_legacy:2",revoked);FamilyLegacyActivationFact activated;activated.consent_fact_id="family_legacy:1";activated.tick=5;state.activations.insert("family_legacy:3",activated);
    FamilyEffect receipt;receipt.success=true;receipt.fact_id="family_legacy:1";receipt.heir_actor_id="next";receipt.player_actor_id="former";
    check(validate_family_legacy_receipt(state,"family_legacy_accept","former",3,receipt).ok);
    check(!validate_family_legacy_receipt(state,"family_legacy_accept","next",3,receipt).ok);
    check(!validate_family_legacy_receipt(state,"family_legacy_accept","former",4,receipt).ok);
    check(!validate_family_legacy_receipt(state,"rest","former",3,receipt).ok);
    for(int field=0;field<8;++field){auto bad=receipt;switch(field){case 0:bad.success=false;break;case 1:bad.already_completed=false;break;case 2:bad.code="ok";break;case 3:bad.actor_id="former";break;case 4:bad.fact_id="family_legacy:99";break;case 5:bad.heir_actor_id="former";break;case 6:bad.player_actor_id="next";break;case 7:bad.fact_id.reset();break;}check(!validate_family_legacy_receipt(state,"family_legacy_accept","former",3,bad).ok);}
    receipt.fact_id="family_legacy:2";check(validate_family_legacy_receipt(state,"family_legacy_revoke","former",4,receipt).ok);check(!validate_family_legacy_receipt(state,"family_legacy_accept","former",4,receipt).ok);
    receipt.fact_id="family_legacy:3";receipt.player_actor_id="next";check(validate_family_legacy_receipt(state,"family_legacy_activate","former",5,receipt).ok);check(!validate_family_legacy_receipt(state,"family_legacy_activate","next",5,receipt).ok);receipt.player_actor_id="former";check(!validate_family_legacy_receipt(state,"family_legacy_activate","former",5,receipt).ok);
    std::cout<<"FAMILY_LEGACY_RECEIPT "<<checks<<" checks "<<failures<<" failures\n";return failures?1:0;
}
