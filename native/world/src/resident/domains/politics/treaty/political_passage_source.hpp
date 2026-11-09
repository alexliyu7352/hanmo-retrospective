#pragma once
#include "political_passage_decision.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
struct ActualPassageEntry {
    Integer entry_ordinal=0,composition_revision=0,manifest_revision=0;
    WorldMoment occurred_at;
};
struct PoliticalPassageDecisionSource {
    StableId id;MovementExecutionRef execution;SpatialCarrierRef carrier;
    Integer entry_ordinal=0,composition_revision=0,manifest_revision=0;
    WorldMoment occurred_at;PoliticalPassageDecision decision;
};
struct PoliticalPassageDecisionState {OrderedTable<PoliticalPassageDecisionSource> decisions;};
class PoliticalPassageEntryRead {
public:
    virtual ~PoliticalPassageEntryRead()=default;
    // S1真正执行/首进入读取；不得由命令随意填ordinal或时刻。
    virtual Outcome<ActualPassageEntry> read_actual_entry(const PassageRequest&,const TransportManifestRead&)const=0;
    virtual Status validate_historical_entry(const PoliticalPassageDecisionSource&)const=0;
};
class PoliticalPassageHistoryRead {
public:
    virtual ~PoliticalPassageHistoryRead()=default;
    // 反读签发时原控制/条约/送达/价目，不用今日政策重判已发生的跨界。
    virtual Status validate_historical_decision(const PoliticalPassageDecisionSource&)const=0;
};
Outcome<SourceRef> adopt_political_passage_decision_in_candidate(CommonStore&,PoliticalPassageDecisionState&,
    const PoliticalPassageContext&,const PoliticalPassageEntryRead&,const PassageRequest&,const TransportManifestRead&);
Outcome<const PoliticalPassageDecisionSource*> read_political_passage_decision(const CommonStore&,const PoliticalPassageDecisionState&,const SourceRef&);
Status validate_political_passage_sources(const CommonStore&,const PoliticalPassageDecisionState&,const PoliticalPassageEntryRead&,const PoliticalPassageHistoryRead&);
// 明版原 source2 行时间门与 paired 入口；旧 source1 visitor 不改变原字节。
Status validate_political_passage_decision_source_time(const PoliticalPassageDecisionSource&);
Status visit_political_passage_sources2(CodecVisitor&,PoliticalPassageDecisionState&);
Outcome<BoundaryValue> encode_political_passage_sources2(const PoliticalPassageDecisionState&);
Outcome<PoliticalPassageDecisionState> decode_political_passage_sources2(const BoundaryValue&);
// 仅M-PASSAGE adopted_decisions叶；完整/3还须同owner SafeExit三表，不能据本叶启用整段。
Status visit_political_passage_sources(CodecVisitor&,PoliticalPassageDecisionState&);
}
