// 师承本人节点来源；只证明明确发言与当面身份，不建立关系或授予课程、继任和财产权。
#pragma once
#include "resident/domains/interaction/dialogue_node_source.hpp"
#include "actor_age_birth_read.hpp"
#include "actor_presence_read.hpp"
namespace hanmo::resident {
class FamilyContinuationPeopleRead4;
// 未限定听者仅指本次真实受控听者；发言者仍为唯一原作者人物，不是任意代言人。
struct ApprenticeshipStatementDefinition {
    StableId id,speaker_actor_id,speaker_side,skill_id,dialogue_id,node_id;
    std::optional<StableId> listener_actor_id;
    Integer terms_version=1;
};
struct ApprenticeshipCatalog {
    bool enabled=false;
    OrderedTable<ApprenticeshipStatementDefinition> statements;
};
// 永久来源保存真实双方和条款，不保存恢复前 epoch；当前报价另持完整原节点读戳。
struct ApprenticeshipStatementSource {
    StableId statement_id,speaker_actor_id,listener_actor_id,teacher_actor_id,student_actor_id;
    StableId skill_id,dialogue_id,node_id,node_kind,session_id,scenario_id;
    WorldId world_id{};
    InteractionContext context;
    Integer terms_version=1,tick=0,revision=0;
    FieldOrder field_order;
};
struct ApprenticeshipStatementQuote {
    AuthoredNodeSourceProof proof;
    ApprenticeshipStatementSource source;
};
// 此访问器只借本候选永久来源，不形成任何人物副本或隐含完整 World 验证。
struct ApprenticeshipIdentityRead {
    const FamilyBirthState* births=nullptr;
    FamilyDeathOriginRead death_origin;
    std::function<Outcome<StableId>(Integer,const StableId&)> player_before;
    std::function<Status(const StableId&,Integer,const StableId&)> personally_available;
    const FamilyContinuationPeopleRead4* complete_people=nullptr;
};
class CodecVisitor;
Status validate_apprenticeship_catalog(const ApprenticeshipCatalog&,const CommonCatalog&,const InteractionCatalog&);
Status validate_apprenticeship_source_shape(const ApprenticeshipStatementSource&);
bool apprenticeship_source_equal(const ApprenticeshipStatementSource&,const ApprenticeshipStatementSource&);
// 当前/历史两口都沿实际出生与原死因；历史入口要求调用方先核本域头及 payload.tick。
Status validate_apprenticeship_person(const CommonStore&,const CommonCatalog&,const ApprenticeshipIdentityRead&,const StableId& actor,Integer tick,const StableId& actual_before_fact={});
Outcome<ApprenticeshipStatementQuote> preview_apprenticeship_statement(const CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const ApprenticeshipCatalog&,const ApprenticeshipIdentityRead&,const StableId& listener,const StableId& statement_id);
Status revalidate_apprenticeship_statement(const CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const ApprenticeshipCatalog&,const ApprenticeshipIdentityRead&,const ApprenticeshipStatementQuote&);
// 冷验不要求旧会话仍打开；当时控制者、真实双方与原节点条款必须逐原事实证明。
Status validate_apprenticeship_statement_source(const CommonStore&,const CommonCatalog&,const InteractionCatalog&,const ApprenticeshipCatalog&,const ApprenticeshipIdentityRead&,const ApprenticeshipStatementSource&,const StableId& actual_before_fact);
Outcome<ApprenticeshipCatalog> decode_apprenticeship_catalog(const BoundaryValue&);
Outcome<ApprenticeshipStatementSource> decode_apprenticeship_statement_source(const BoundaryValue&);
BoundaryValue encode_apprenticeship_statement_source(const ApprenticeshipStatementSource&);
Status visit_apprenticeship_statement_source(CodecVisitor&,ApprenticeshipStatementSource&);
}
