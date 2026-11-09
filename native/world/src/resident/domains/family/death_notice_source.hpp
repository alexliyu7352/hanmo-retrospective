// 死亡信件内容只取真实亲历；此证明不表示已发信、已送达或受益人已同意受领。
#pragma once
#include "family_death.hpp"
#include "resident/model/dynamic_contract.hpp"
namespace hanmo::resident {
// 军事完整历史预验后给出的有限同部/同船证明，不从当前位置或全军名单推知情。
struct FamilyDeathWitness {
    StableId observer_actor_id,dead_actor_id,casualty_fact_id,battle_id,unit_id;Integer observed_tick=0;
};
using FamilyDeathWitnessRead=std::function<Outcome<std::optional<FamilyDeathWitness>>(const StableId& observer,const StableId& dead,const StableId& casualty)>;
// Task原受领账负责notice/Task/实物信件身份和发信委托；本结构只冻结信内来源。
struct WitnessedDeathNoticeSource {
    StableId version="witnessed_death_v1",scenario_id,death_fact_id,decedent_actor_id,sender_actor_id,recipient_actor_id;
    StableId casualty_fact_id,battle_id,unit_id;WorldId world_id{};Integer observed_tick=0,issued_tick=0;
    FieldOrder field_order;
};
// 仅任务原受领边界调用；phase非空必须是当前半日from/to，不能任意声明未来。
using WitnessedDeathNoticeRead=std::function<Outcome<WitnessedDeathNoticeSource>(const StableId& sender,const StableId& recipient,const StableId& decedent,Integer accept_tick,const DynamicStepInput* phase)>;
Outcome<WitnessedDeathNoticeSource> read_witnessed_death_notice(const CommonStore&,const FamilyDeathState&,const FamilyDeathOriginRead&,const FamilyDeathWitnessRead&,const StableId& sender,const StableId& recipient,const StableId& decedent,Integer accept_tick,const DynamicStepInput* phase=nullptr);
struct WitnessedDeathNoticeQuote { WorldIdentity identity;WitnessedDeathNoticeSource source; };
// 调用方先验证完整FamilyLife及军事/Naval历史，并证明原送信委托的本人发信意图。
// 仅原受领当前时刻可取新源；不扫描所有死亡生成消息，也不向携信人授权知情。
Outcome<WitnessedDeathNoticeQuote> preview_witnessed_death_notice(const CommonStore&,const FamilyDeathState&,const FamilyDeathOriginRead&,const FamilyDeathWitnessRead&,const StableId& death_fact_id,const StableId& sender,const StableId& recipient);
Status revalidate_witnessed_death_notice(const CommonStore&,const FamilyDeathState&,const FamilyDeathOriginRead&,const FamilyDeathWitnessRead&,const WitnessedDeathNoticeQuote&);
// 冷读须给真实Task受领/绑定事实边界，且调用方核该payload.tick==issued_tick。
// 发送者后来死亡不抹旧信；边界前已死亡则拒，同tick按Common实际顺序而非数字ID。
Status validate_witnessed_death_notice(const CommonStore&,const FamilyDeathState&,const FamilyDeathOriginRead&,const FamilyDeathWitnessRead&,const WitnessedDeathNoticeSource&,const StableId& binding_fact_id,const DynamicStepInput* phase=nullptr);
bool witnessed_death_notice_equal(const WitnessedDeathNoticeSource&,const WitnessedDeathNoticeSource&);
// S3新绑定段复用本来源的严格JSON/typed叶，不把任意JSON当作知情凭据。
Outcome<WitnessedDeathNoticeSource> decode_witnessed_death_notice_source(const BoundaryValue&);
class ProjectionWriter;
Status emit_witnessed_death_notice_source_fields(ProjectionWriter&,const WitnessedDeathNoticeSource&);
BoundaryValue encode_witnessed_death_notice_source(const WitnessedDeathNoticeSource&);
Status visit_witnessed_death_notice_source(CodecVisitor&,WitnessedDeathNoticeSource&);
}
