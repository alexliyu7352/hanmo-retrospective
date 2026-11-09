// 报价复核直接比较类型化业务值；字段顺序只属冷边界，不在热路径编码字典或JSON。
#pragma once
#include "growth_model.hpp"
#include <tuple>
namespace hanmo::resident::growth {
    inline bool context_equal(const InteractionContext& a,const InteractionContext& b){return std::tie(a.place_id,a.provider_actor_id,a.role_id,a.kind,a.encounter_id,a.input_error)==std::tie(b.place_id,b.provider_actor_id,b.role_id,b.kind,b.encounter_id,b.input_error);
    }
    inline bool contexts_equal(const std::optional<InteractionContext>& a,const std::optional<InteractionContext>& b){return bool(a)==bool(b)&&(!a||context_equal(*a,*b));
    }
    // 原公开题面上下文连同可空真实quote绑定逐值核对，检查点不能替换预算、关系折扣或时效。
    inline bool challenge_context_equal(const std::optional<StudyChallengeContext>& a,const std::optional<StudyChallengeContext>& b){
        if(bool(a)!=bool(b))return false;
        if(!a)return true;
        if(std::tie(a->budget,a->quantity,a->relation_discount_bp)!=std::tie(b->budget,b->quantity,b->relation_discount_bp)||bool(a->quote)!=bool(b->quote))return false;
        return !a->quote||std::tie(a->quote->quote_id,a->quote->quote_sha256,a->quote->market_revision,a->quote->valid_tick)==std::tie(b->quote->quote_id,b->quote->quote_sha256,b->quote->market_revision,b->quote->valid_tick);
    }
    inline bool frozen_equal(const ChallengeFrozen& a,const ChallengeFrozen& b){if(std::tie(a.field,a.kind,a.challenge_id,a.mode,a.source_sha256,a.skill_level,a.auxiliary_attribute)!=std::tie(b.field,b.kind,b.challenge_id,b.mode,b.source_sha256,b.skill_level,b.auxiliary_attribute)||bool(a.context)!=bool(b.context))return false;
        return challenge_context_equal(a.context,b.context);
    }
    inline bool training_equal(const TrainingTerms& a,const TrainingTerms& b){return std::tie(a.skill_id,a.student_id,a.teacher_id,a.place_id,a.payer_wallet_id,a.recipient_wallet_id,a.duration,a.mastery_before,a.mastery_after,a.student_fatigue,a.teacher_fatigue,a.fee)==std::tie(b.skill_id,b.student_id,b.teacher_id,b.place_id,b.payer_wallet_id,b.recipient_wallet_id,b.duration,b.mastery_before,b.mastery_after,b.student_fatigue,b.teacher_fatigue,b.fee)&&bool(a.teaching_agreement)==bool(b.teaching_agreement)&&(!a.teaching_agreement||growth_teaching_agreement_equal(*a.teaching_agreement,*b.teaching_agreement));
    }
    inline bool study_equal(const StudyTerms& a,const StudyTerms& b){if(std::tie(a.skill_id,a.mode,a.student_id,a.teacher_id,a.payer_wallet_id,a.recipient_wallet_id,a.duration,a.xp_before,a.teacher_xp,a.student_fatigue,a.teacher_fatigue,a.xp_limit,a.fee,a.settlement_policy)!=std::tie(b.skill_id,b.mode,b.student_id,b.teacher_id,b.payer_wallet_id,b.recipient_wallet_id,b.duration,b.xp_before,b.teacher_xp,b.student_fatigue,b.teacher_fatigue,b.xp_limit,b.fee,b.settlement_policy)||!context_equal(a.context,b.context)||bool(a.challenge)!=bool(b.challenge))return false;
        if(bool(a.study_teaching_agreement)!=bool(b.study_teaching_agreement)|| (a.study_teaching_agreement&&!growth_study_teaching_agreement_equal(*a.study_teaching_agreement,*b.study_teaching_agreement)))return false;
        if(bool(a.innate_source)!=bool(b.innate_source)||(a.innate_source&&!innate_study_source_equal(*a.innate_source,*b.innate_source)))return false;
        return !a.challenge||frozen_equal(*a.challenge,*b.challenge);
    }
    inline bool quote_equal(const GrowthQuote& a,const GrowthQuote& b){if(a.data.index()!=b.data.index()||std::tie(a.duration,a.cost,a.wage,a.expected_xp,a.target_level)!=std::tie(b.duration,b.cost,b.wage,b.expected_xp,b.target_level)||!contexts_equal(a.context,b.context))return false;
        return std::visit([&](const auto& x){using T=std::decay_t<decltype(x)>;
            const auto& y=std::get<T>(b.data);
            if constexpr(std::is_same_v<T,TrainingTerms>)return training_equal(x,y);
            else if constexpr(std::is_same_v<T,StudyTerms>)return study_equal(x,y);
            else if constexpr(std::is_same_v<T,ServiceTerms>)return std::tie(x.duration,x.cost,x.wage,x.hp_gain,x.fatigue_delta,x.wallet_id)==std::tie(y.duration,y.cost,y.wage,y.hp_gain,y.fatigue_delta,y.wallet_id);
            else if constexpr(std::is_same_v<T,TradeTerms>)return std::tie(x.shop_id,x.item_id,x.side,x.payer_wallet_id,x.quantity,x.total,x.market_revision,x.valid_tick)==std::tie(y.shop_id,y.item_id,y.side,y.payer_wallet_id,y.quantity,y.total,y.market_revision,y.valid_tick)&&owner_equal(x.owner,y.owner)&&context_equal(x.context,y.context)&&x.bargain_quote_id==y.bargain_quote_id&&x.bargain_basis_sha256==y.bargain_basis_sha256&&x.discount_bp==y.discount_bp;
            else if constexpr(std::is_same_v<T,EquipmentChoice>)return x.id==y.id&&x.defense==y.defense;
            else if constexpr(std::is_same_v<T,StudyCancellation>)return std::tie(x.action_id,x.completed_lessons,x.fee_paid,x.unprovided_released)==std::tie(y.action_id,y.completed_lessons,y.fee_paid,y.unprovided_released);
            else if constexpr(std::is_same_v<T,StudyPauseChange>)return std::tie(x.action_id,x.paused,x.paused_ticks,x.completed_lessons,x.reserved)==std::tie(y.action_id,y.paused,y.paused_ticks,y.completed_lessons,y.reserved);
            else if constexpr(std::is_same_v<T,RestTerms>)return std::tie(x.estate_id,x.food_item_id,x.duration,x.ration_per_person,x.mouths)==std::tie(y.estate_id,y.food_item_id,y.duration,y.ration_per_person,y.mouths);
            else if constexpr(std::is_same_v<T,RestCancellation>)return x.action_id==y.action_id;
            else return false;
        },a.data);
    }
} // 只比较已存在旧合同字段，不创造新指纹或第二版本号。
