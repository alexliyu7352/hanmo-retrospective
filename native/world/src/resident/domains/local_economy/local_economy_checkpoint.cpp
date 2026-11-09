// 地方经济检查点遍历全部持久字段；恢复不重放资源结果。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "local_economy_codec_detail.hpp"
#include "born_population_sources.hpp"
namespace hanmo::resident {
namespace {
Status visit_selected_checkpoint(CodecVisitor& codec,LocalEconomyState& state,bool military_adapter,bool named_membership,bool production_management,bool household_provisioning,bool born_population) {if(!household_provisioning&&state.household_provisioning)return codec.reject("HouseholdProvisionCodecRequired","local_economy");if(!production_management&&(state.production_management||std::any_of(state.production_receipts.rows().begin(),state.production_receipts.rows().end(),[](const auto& row){return row.second.production_context.has_value();})))return codec.reject("ProductionManagementCodecRequired","local_economy");if(!born_population&&!state.named_birth_population_transitions.empty())return codec.reject("NamedBirthPopulationSelectedSourceRequired","local_economy");return dynamic_economy_codec::checkpoint(codec,"local_economy",state,military_adapter,named_membership,production_management,household_provisioning,born_population);}
}
Status visit_local_economy_checkpoint(CodecVisitor& codec,LocalEconomyState& state,bool military_adapter,bool named_membership,bool production_management,bool household_provisioning){return visit_selected_checkpoint(codec,state,military_adapter,named_membership,production_management,household_provisioning,false);}

Status visit_local_economy_checkpoint(CodecVisitor&codec,LocalEconomyState&state,bool military,bool named,bool management,bool household,Integer selected){
 auto valid=validate_local_economy_codec_selection(selected,military,named,management,household);if(!valid.ok)return codec.reject(valid.error.original.code,valid.error.original.path);
 return visit_selected_checkpoint(codec,state,military,named,management,household,named_birth_population_selected(selected));
}

}
