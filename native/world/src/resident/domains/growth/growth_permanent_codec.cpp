#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "growth_permanent_section.hpp"
#include "card_acquisition_codec_fields.hpp"
#include "growth_permanent_codec_fields.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/storage/common_checkpoint.hpp"
namespace hanmo::resident {namespace {
template<class T,class F>Status table(CodecVisitor& c,const StableId& name,OrderedTable<T>& rows,F fn){return common_checkpoint::table(c,name,rows,fn);}
template<class C,class T,class F>Status table(C& c,const StableId& name,OrderedTable<T>& rows,F fn){return c.table(name,rows,fn);}
template<class C>Status fields(C& c,GrowthPermanentState& value,bool capability_chain=false,GrowthContributionWireVersion wire=GrowthContributionWireVersion::original){table(c,"milestones",value.milestones,[&](auto& x,auto& r){return growth_permanent_codec::milestone(x,r,capability_chain,wire);});table(c,"practice_awards",value.practice_awards,[&](auto& x,auto& r){return growth_permanent_codec::award(x,r,capability_chain,wire);});return table(c,"capability_revisions",value.capability_revisions,[&](auto& x,auto& r){return growth_permanent_codec::revision(x,r,wire);});}
}
Status visit_growth_permanent_contributions(CodecVisitor& c,GrowthPermanentState& value){return fields(c,value);}
Status emit_growth_permanent_contributions_projection(ProjectionWriter& out,std::string_view name,const GrowthPermanentState& value){return emit_projection_record<ActorFoundationProjectionArchive>(out,name,value,[](auto& c,auto& x){return fields(c,x);});}
Outcome<BoundaryValue> encode_growth_permanent_contributions(const GrowthPermanentState& value){BoundaryValue out;BoundaryProjectionWriter writer(out);auto status=emit_growth_permanent_contributions_projection(writer,"",value);if(status.ok)status=writer.finish();return {status.ok,std::move(out),status.error};}
Outcome<GrowthPermanentState> decode_growth_permanent_contributions(const BoundaryValue& input){GrowthPermanentState value;ActorFoundationJsonVisitor reader(input);auto status=fields(reader,value);if(status.ok)status=reader.finish();return {status.ok,std::move(value),status.error};}
}

namespace hanmo::resident {
Status visit_growth_permanent_contributions_v2(CodecVisitor& c,GrowthPermanentState& value){return fields(c,value,true);}
Status emit_growth_permanent_contributions_v2_projection(ProjectionWriter& out,std::string_view name,const GrowthPermanentState& value){return emit_projection_record<ActorFoundationProjectionArchive>(out,name,value,[](auto& c,auto& x){return fields(c,x,true);});}
Outcome<BoundaryValue> encode_growth_permanent_contributions_v2(const GrowthPermanentState& value){BoundaryValue out;BoundaryProjectionWriter writer(out);auto status=emit_growth_permanent_contributions_v2_projection(writer,"",value);if(status.ok)status=writer.finish();return {status.ok,std::move(out),status.error};}
Outcome<GrowthPermanentState> decode_growth_permanent_contributions_v2(const BoundaryValue& input){GrowthPermanentState value;ActorFoundationJsonVisitor reader(input);auto status=fields(reader,value,true);if(status.ok)status=reader.finish();return {status.ok,std::move(value),status.error};}
}

namespace hanmo::resident {namespace {
template<class C>Status section(C& c,GrowthPermanentState& permanent,CardAcquisitionState& cards,GrowthPermanentSectionVersion version){
 if(version!=GrowthPermanentSectionVersion::original&&version!=GrowthPermanentSectionVersion::capability_application_chain&&version!=GrowthPermanentSectionVersion::trade_grants)return c.reject("GrowthPermanentSectionVersionUnsupported");
 auto valid=fields(c,permanent,version!=GrowthPermanentSectionVersion::original,version==GrowthPermanentSectionVersion::trade_grants?GrowthContributionWireVersion::trade_grants:GrowthContributionWireVersion::original);if(!valid.ok)return valid;
 return visit_card_acquisition_fields(c,cards);
}
}
Status visit_growth_permanent_section(CodecVisitor& c,GrowthPermanentState& permanent,CardAcquisitionState& cards,GrowthPermanentSectionVersion version){return section(c,permanent,cards,version);}
Status emit_growth_permanent_section_projection(ProjectionWriter& out,std::string_view name,const GrowthPermanentState& permanent,const CardAcquisitionState& cards,GrowthPermanentSectionVersion version){
 return out.object(name,[&](ProjectionWriter& child){ActorFoundationProjectionArchive c(child);return section(c,const_cast<GrowthPermanentState&>(permanent),const_cast<CardAcquisitionState&>(cards),version);});
}
Outcome<BoundaryValue> encode_growth_permanent_section(const GrowthPermanentState& permanent,const CardAcquisitionState& cards,GrowthPermanentSectionVersion version){BoundaryValue out;BoundaryProjectionWriter writer(out);auto valid=emit_growth_permanent_section_projection(writer,"",permanent,cards,version);if(valid.ok)valid=writer.finish();return {valid.ok,std::move(out),valid.error};}
Outcome<DecodedGrowthPermanentSection> decode_growth_permanent_section(const BoundaryValue& input,GrowthPermanentSectionVersion version){DecodedGrowthPermanentSection candidate;ActorFoundationJsonVisitor reader(input);auto valid=section(reader,candidate.permanent,candidate.cards,version);if(valid.ok)valid=reader.finish();return {valid.ok,std::move(candidate),valid.error};}
}
