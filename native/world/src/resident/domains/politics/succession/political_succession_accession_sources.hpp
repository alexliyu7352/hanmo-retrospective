// 正式就任前的真实死亡、本人获知和原首职空缺来源；不直接更换Office或控制者。
#pragma once
#include "political_succession_nominations.hpp"
#include "resident/domains/family/actor_death_read.hpp"
#include "resident/domains/tasks/message_notices/task_message_notice.hpp"
namespace hanmo::resident {
struct PoliticalSuccessionAccessionBasis {
 WorldIdentity source_identity;
 StableId nomination_fact_id,acceptance_fact_id,policy_id,organization_id,office_id;
 StableId predecessor_actor_id,successor_actor_id,death_fact_id,death_cause_fact_id;
 StableId notice_binding_fact_id,notice_handoff_fact_id,vacancy_fact_id,before_fact_id;
 Integer death_tick=0,received_tick=0,vacant_office_epoch=0,observed_tick=0;
};
// 所借意愿、FamilyLife与Task通知账须分别完成原全验；本口只交叉核指定来源与真实截面。
struct PoliticalSuccessionAccessionSourceContext {
 const PoliticalSuccessionNominationContext& nominations;
 const FamilyDeathState& deaths;
 TaskDeliveredDeathNoticeRead delivered_notice;
};
// 首片只认原文书已实交本人，实际死亡和原Office首次空缺之后才具备候选就任基础。
Outcome<PoliticalSuccessionAccessionBasis> read_political_succession_accession_basis(const PoliticalSuccessionAccessionSourceContext&,const StableId& acceptance_fact_id,const StableId& death_fact_id,const StableId& notice_binding_fact_id);
// 后来通知、后来死亡登记或同tick移序不能倒填旧就任；边界仅本业务真实就任头。
Outcome<PoliticalSuccessionAccessionBasis> read_political_succession_accession_basis_before(const PoliticalSuccessionAccessionSourceContext&,const StableId& acceptance_fact_id,const StableId& death_fact_id,const StableId& notice_binding_fact_id,Integer tick,const StableId& actual_accession_fact);
}
