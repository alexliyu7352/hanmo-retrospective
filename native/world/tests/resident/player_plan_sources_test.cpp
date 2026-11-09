// 精确条款回归：报价漂移必须停止，普通计数变化不得伪造新报价。
#include "resident/domains/player_plan/plan_sources.hpp"
#include <cassert>
#include <iostream>
using namespace hanmo::resident;
namespace {
// 夹具保原GrowthQuote结构，避免测试另一个近似报价模型。
GrowthQuote course(){GrowthQuote q;q.command.kind="study_skill";q.command.skill_id="rites";q.command.ticks=2;q.command.challenge_mode="manual";q.duration=2;q.cost=60;q.target_level=1;q.context=InteractionContext{"school","teacher","school_role",{},{},{},{}};q.command.context=q.context;StudyTerms t;t.skill_id="rites";t.mode="teacher";t.student_id="player";t.teacher_id="teacher";t.context=*q.context;t.duration=2;t.xp_before=10;t.teacher_xp=650;t.student_fatigue=5;t.teacher_fatigue=10;t.xp_limit=650;t.fee=60;t.payer_wallet_id="player_wallet";t.recipient_wallet_id="school_wallet";t.settlement_policy="per_lesson_v1";ChallengeFrozen challenge;challenge.kind="etiquette";challenge.field="etiquette";challenge.challenge_id="greeting";challenge.mode="manual";challenge.source_sha256="same_source";t.challenge=challenge;q.data=t;return q;}
// 更换教师、涨价、改课程参数/模式/资格上限均不得被总预算吸收。
void exact_terms(){auto initial=course();auto frozen=freeze_player_plan_terms(initial,"player_wallet");assert(frozen.ok);auto fresh=initial;auto& t=std::get<StudyTerms>(fresh.data);t.xp_before=20;t.teacher_xp=670;t.student_fatigue=35;t.teacher_fatigue=42;t.challenge->skill_level=1;t.challenge->auxiliary_attribute=50;fresh.expected_xp=21;assert(match_player_plan_terms(frozen.value,fresh,"player_wallet").ok);
    auto drift=initial;std::get<StudyTerms>(drift.data).fee=80;drift.cost=80;assert(!match_player_plan_terms(frozen.value,drift,"player_wallet").ok);
    drift=initial;drift.context->provider_actor_id="replacement";drift.command.context=drift.context;std::get<StudyTerms>(drift.data).teacher_id="replacement";std::get<StudyTerms>(drift.data).context=*drift.context;assert(!match_player_plan_terms(frozen.value,drift,"player_wallet").ok);
    drift=initial;std::get<StudyTerms>(drift.data).xp_limit=300;assert(!match_player_plan_terms(frozen.value,drift,"player_wallet").ok);
    drift=initial;std::get<StudyTerms>(drift.data).challenge->source_sha256="changed_source";assert(!match_player_plan_terms(frozen.value,drift,"player_wallet").ok);
    drift=initial;std::get<StudyTerms>(drift.data).challenge->context=StudyChallengeContext{100,2,0,{},{}};assert(!match_player_plan_terms(frozen.value,drift,"player_wallet").ok);
    drift=initial;std::get<StudyTerms>(drift.data).challenge->mode="automatic";drift.command.challenge_mode="automatic";assert(!match_player_plan_terms(frozen.value,drift,"player_wallet").ok);
    drift=initial;std::get<StudyTerms>(drift.data).duration=3;drift.duration=3;drift.command.ticks=3;assert(!match_player_plan_terms(frozen.value,drift,"player_wallet").ok);
}
// 只有原普通短工能冻结服务授权；叙事上下文、交易和武学不是生活队列入口。
void finite_work(){GrowthQuote q;q.command.kind="service";q.command.activity="work";q.context=InteractionContext{"market","employer","labor_role",{},{},{},{}};q.command.context=q.context;q.duration=1;q.cost=3;q.wage=30;q.data=ServiceTerms{1,3,30,0,10,"employer_wallet",{}};auto frozen=freeze_player_plan_terms(q,"player_wallet");assert(frozen.ok&&frozen.value.cost==3&&frozen.value.wage==30);auto drift=q;std::get<ServiceTerms>(drift.data).wage=31;drift.wage=31;assert(!match_player_plan_terms(frozen.value,drift,"player_wallet").ok);q.command.activity="rest";assert(!freeze_player_plan_terms(q,"player_wallet").ok);q.command.kind="trade";assert(!freeze_player_plan_terms(q,"player_wallet").ok);q=course();q.context->kind="narrative";assert(!freeze_player_plan_terms(q,"player_wallet").ok);}
}
int main(){exact_terms();finite_work();std::cout<<"player_plan_sources: PASS (price/provider/qualification/parameters/challenge source drift; live counters; finite source types)\n";}
