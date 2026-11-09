// 禀赋只影响亲历课时的有界学习速度；未知档位不当作0档，也不从后天经验反推。
#pragma once
#include "family_birth_identity.hpp"
#include "innate_study_model.hpp"
namespace hanmo::resident {
// 只供已验证同一World的业务规则；不是向任意观察者公开隐秘禀赋的查询页。
Outcome<InnateStudyRead> read_actor_innate_study_source(const CommonStore&,const CommonCatalog&,const FamilyState&,const FamilyCatalog&,const FamilyLineageCatalog&,const FamilyBirthState&,const StableId& actor_id,const StableId& skill_id);
// 边界须为真实skill_study/cultivation行动头；调用Growth域另核原payload的start_tick等于tick。
Outcome<InnateStudyRead> read_actor_innate_study_source_before(const CommonStore&,const CommonCatalog&,const FamilyState&,const FamilyCatalog&,const FamilyLineageCatalog&,const FamilyBirthState&,const StableId& actor_id,const StableId& skill_id,Integer tick,const StableId& study_action_id);
Status validate_innate_study_source(const CommonStore&,const CommonCatalog&,const FamilyState&,const FamilyCatalog&,const FamilyLineageCatalog&,const FamilyBirthState&,const InnateStudySource&,const StableId& study_action_id);
Outcome<Integer> innate_study_lesson_xp(Integer original_base_xp,Integer bonus_bp);
Outcome<InnateStudySource> decode_innate_study_source(const BoundaryValue&);
BoundaryValue encode_innate_study_source(const InnateStudySource&);
Status visit_innate_study_source(CodecVisitor&,InnateStudySource&);
}
