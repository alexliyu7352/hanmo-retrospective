// 家事共享冷codec和typed相等工具只在同域使用；运行相等不转JSON。
#pragma once
#include "family_model.hpp"
namespace hanmo::resident::family_codec {
FamilyPolicy policy(const BoundaryValue&);
AdoptionTerms terms(const BoundaryValue&);
SuccessionAssets assets(const BoundaryValue&);
BoundaryValue encode_policy(const FamilyPolicy&);
BoundaryValue encode_terms(const AdoptionTerms&);
BoundaryValue encode_assets(const SuccessionAssets&);
BoundaryValue encode_choice(const FamilyChoice&,bool fact);
bool same_policy(const FamilyPolicy&,const FamilyPolicy&);
bool same_terms(const AdoptionTerms&,const AdoptionTerms&);
bool same_choice(const FamilyChoice&,const FamilyChoice&);
} // 字段序不参与业务相等，字段存在位参与。
