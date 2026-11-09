#include "social_response_rules.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {namespace {
template<class T>Outcome<T> invalid(const char*code){return {false,{},common_failure(code,{},ErrorKind::internal).error};}
}
Outcome<OriginalSocialResponseScore> score_original_social_response(const OriginalSocialScoreInputs&i){
 // Formal03 sections3.1/3.4: effective attributes are1..110 and the
 // actual request difficulty is0..60. The original XP thresholds give0..4.
 if(i.effective_trust<0||i.effective_trust>100||i.effective_affinity<0||i.effective_affinity>100||
    i.auxiliary_attribute<1||i.auxiliary_attribute>110||i.skill_level<0||i.skill_level>4||
    i.interest_modifier < -20||i.interest_modifier>20||i.difficulty<0||i.difficulty>60)return invalid<OriginalSocialResponseScore>("InvalidOriginalSocialScoreInput");
 // Each nonnegative weighted term is floored separately, as in the original
 // formula. Combining the numerators before division would change its result.
 const Integer base=i.effective_affinity*20/100+i.effective_trust*35/100+
                    i.auxiliary_attribute*15/100+5*i.skill_level+i.interest_modifier;
 // The original finite input ranges make base-difficulty bounded[-80,111].
 const Integer score=base-i.difficulty;
 return {true,{score,score>=40?OriginalSocialResponseBand::accept:score<=10?OriginalSocialResponseBand::refuse:OriginalSocialResponseBand::negotiate},{}};
}
Outcome<OriginalSocialResponseScore> finish_original_social_negotiation(Integer score,Integer delta){
 if(score<=10||score>=40||delta < -5||delta>5)return invalid<OriginalSocialResponseScore>("InvalidOriginalSocialNegotiationInput");
 const Integer final_score=score+delta;
 return {true,{final_score,final_score>=30?OriginalSocialResponseBand::accept:OriginalSocialResponseBand::refuse},{}};
}
Outcome<Integer> original_social_family_credit(std::optional<Integer> standing){
 if(!standing)return {true,0,{}};
 if(*standing < -1000||*standing>1000)return invalid<Integer>("InvalidOriginalFamilyStanding");
 // C++ signed division truncates toward zero. The original bounded input makes
 // this already lie in [-5,5]; no floor for negative family standing is used.
 return {true,*standing/200,{}};
}
}
