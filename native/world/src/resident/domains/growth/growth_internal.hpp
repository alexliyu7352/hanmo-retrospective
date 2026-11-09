// 成长领域内部辅助；所有热函数只消费有类型值，边界编解码不参与规则推进。
#pragma once
#include "growth_model.hpp"
namespace hanmo::resident { class GrowthFixedStepPermit; class PreparedTerminalRecoveryV1; struct GrowthExactStartContext; }
namespace hanmo::resident::growth {
    // Original ordinary Study tariff; accepted XP is frozen before the course.
    // Shared by quote, actual lesson payment, termination and cold/read views.
    Integer original_study_lesson_cost(Integer accepted_xp_before);
    // Original Growth numeric rules shared by execution, cold/read projection
    // and the Study-only CharacterCapability reducer. No author selection here.
    Integer original_skill_threshold(Integer level)noexcept;
    struct OriginalStudyEfficiency {Integer fatigue_cap_bp=0,injury_cap_bp=0,efficiency_bp=0;};
    OriginalStudyEfficiency original_study_efficiency(Integer fatigue,Integer injury)noexcept;
    inline bool study_action_kind(const StableId& kind){return kind=="study_skill"||kind=="teach_study";}
    inline StableId study_initiator(const StudyTerms& terms){return terms.study_teaching_agreement?terms.teacher_id:terms.student_id;}
    inline std::vector<StableId> study_participants(const StudyTerms& terms){return terms.study_teaching_agreement?std::vector<StableId>{terms.teacher_id,terms.student_id}:std::vector<StableId>{terms.student_id,terms.teacher_id};}

    const GrowthCourse* course(const GrowthCatalog&,const StableId&,const StableId&);
    Status ready(const CommonStore&,const GrowthAccess&,const StableId&,const StableId&,const StableId&,bool study);
    Outcome<ChallengeFrozen> challenge_quote(const GrowthCatalog&,const GrowthCourse&,const ActorRecord&,const StableId&);
    Outcome<GrowthChallengeInstance> challenge_start(CommonStore&,const GrowthCatalog&,const ChallengeFrozen&,const StableId&,const StableId&);
    Status challenge_validate(const GrowthCatalog&,const GrowthChallengeInstance&);
    Outcome<GrowthChallengeInstance> challenge_transition(const GrowthCatalog&,const GrowthChallengeInstance&,const GrowthCommand&);
    Status challenge_close(const GrowthCatalog&,GrowthChallengeInstance&);
    // 题面状态仅供展示；未决课程沿正常基线教学，已决倍率保持原公式。
    bool challenge_pending(const GrowthChallengeInstance&);
    Integer challenge_lesson_xp(const StudyFact&,Integer);
    GrowthPublicChallenge public_challenge(const GrowthCatalog&,const CommonCatalog&,const GrowthChallengeInstance&);
    BoundaryValue encode_public_challenge(const GrowthPublicChallenge&);
    GrowthPublicChallenge decode_public_challenge(const BoundaryValue&,const StableId&,const StableId&);
    GrowthChallengeResult decode_challenge_result(const BoundaryValue&,const StableId&,const StableId&);
    BoundaryValue challenge_view(const GrowthCatalog&,const CommonCatalog&,const GrowthChallengeInstance&);
    BoundaryValue encode_challenge(const GrowthChallengeInstance&);
    GrowthChallengeInstance decode_challenge(const BoundaryValue&,const ChallengeFrozen&,const StableId&);
    BoundaryValue encode_frozen(const ChallengeFrozen&);
    ChallengeFrozen decode_frozen(const BoundaryValue&,const StableId&,const StableId&);
    Status visit_challenge(CodecVisitor&,GrowthChallengeInstance&);
    Status visit_frozen(CodecVisitor&,ChallengeFrozen&);
    Outcome<GrowthQuote> preview_service_trade(const CommonStore&,const GrowthCatalog&,const CommonCatalog&,const GrowthAccess&,const StableId&,const GrowthCommand&,const StableId&);
    Status start_service_trade(CommonStore&,GrowthState&,const GrowthCatalog&,const ActionCoreView&,const GrowthQuote&,MarketBusinessCaptureServices* market_capture=nullptr,std::optional<Integer> actual_started_tick={},const GrowthExactStartContext* actual_start=nullptr);
    Outcome<GrowthEffect> finish_service_trade(CommonStore&,GrowthState&,const GrowthCatalog&,SocialState&,InteractionState&,const ActionCoreView&,const GrowthPersonalRecoveryServices* personal_recovery=nullptr,std::optional<Integer> actual_completed_tick={},OriginalGrowthServiceRelationWrite* relation_writer=nullptr,const PreparedTerminalRecoveryV1* terminal_recovery=nullptr);
    Status validate_service_trade(const CommonStore&,const GrowthCatalog&,const CommonCatalog&,const ActionCoreView&,const GrowthAction&);
    Outcome<GrowthQuote> preview_rest(const CommonStore&,const GrowthState&,const GrowthAccess&,const StableId&,const GrowthCommand&,const StableId&);
    Status start_rest(CommonStore&,GrowthState&,const ActionCoreView&,const GrowthQuote&,std::optional<Integer> actual_started_tick={});
    Outcome<GrowthStep> step_rest(CommonStore&,GrowthState&,const GrowthAccess&,const ActionCoreView&);
    Outcome<GrowthStep> step_rest_at_fixed_gate(CommonStore&,GrowthState&,const GrowthAccess&,const ActionCoreView&,const GrowthFixedStepPermit&);
    Status validate_rest(const CommonStore&,const GrowthState&,const GrowthAccess&,const StableId&,const RestFact&);
    Outcome<GrowthQuote> preview_bargain(const CommonStore&,const GrowthState&,const GrowthCatalog&,const CommonCatalog&,const GrowthAccess&,const StableId&,const GrowthCommand&,const StableId&);
    Outcome<GrowthEffect> apply_bargain(CommonStore&,GrowthState&,const GrowthCatalog&,const CommonCatalog&,const GrowthAccess&,const StableId&,const GrowthQuote&);
    Status validate_bargain(const CommonStore&,const GrowthState&,const GrowthCatalog&,const CommonCatalog&,const StableId&,const BargainFact&);
    Outcome<BoundaryValue> bargain_view(const CommonStore&,const GrowthState&,const GrowthCatalog&,const CommonCatalog&,const GrowthAccess&,const StableId&,const StableId&);
    Integer trade_price(Integer base,Integer stock,Integer quantity,bool buy,Integer discount=0);
    BoundaryValue encode_trade_terms(const TradeTerms&);
    TradeTerms decode_trade_terms(const BoundaryValue&,const StableId&);
    BoundaryValue encode_bargain_market(const BargainMarketBasis&);
    BoundaryValue encode_bargain_terms(const BargainTerms&);
    BargainTerms decode_bargain_terms(const BoundaryValue&,const StableId&);
    BoundaryValue encode_definition(const GrowthChallengeDefinition&);
    StableId bargain_source_digest(const GrowthChallengeDefinition&,const BargainPolicy&,const CommonCatalog&);
    StableId challenge_digest(const GrowthChallengeDefinition&,const CommonCatalog&);
    StableId canonical_digest(BoundaryValue);
    BoundaryValue encode_bargain_fact(const BargainFact&);
    BargainFact decode_bargain_fact(const BoundaryValue&,const StableId&);
    BoundaryValue encode_bargain_prepared(const BargainPrepared&);
    // 暂停证据提供唯一课程时钟和分段疲劳基线，缺失字段保留旧课程语义。
    bool study_paused(const StudyFact&);
    // Same original pause algorithm used by the command and actual input loss.
    Status pause_study_in_candidate(CommonStore&,GrowthState&,const StableId&,const WorldMoment&);
    // 新旧结算政策明确分支；待答信息不阻断原有晚答课程。
    bool study_segmented(const StudyFact&);
    bool study_waiting_challenge(const StudyFact&);
    bool study_pause_supported(const StudyFact&);
    Integer study_paused_ticks(const StudyFact&,Integer now);
    Integer study_fatigue(const StudyFact&,Integer lesson_index,bool teacher=false);
    // 恢复经验只冻结当前段的基线；终态实际贡献不吞掉暂停期间的外部成长。
    Integer study_xp_baseline(const StudyFact&);
    Integer study_xp_gained(const StudyFact&);
    Integer reserved(const WalletView*,const StableId&);
} // 这些辅助不分配世界实例ID，除challenge唯一种子抽取与既有资金lot流程。
