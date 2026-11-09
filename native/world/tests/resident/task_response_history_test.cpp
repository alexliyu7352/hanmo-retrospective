// 复用已验原生冷消费者的真实目录准备，单独核旧profile不能持有新增回应命令历史。
#define main governance_cold_probe_unused_entry
#include "governance_task_checkpoint_probe.cpp"
#undef main
#include "resident/domains/tasks/task_offer_response_profile.hpp"
// 只篡改冷测试候选的空Done回执kind，原银行与安装世界始终只读。
int main(int argc,char** argv){try{
    require(argc==4,"usage: task_response_history_test CATALOG OLD_PROFILE_BANK OUTPUT");const auto catalog=read_json(argv[1]);const auto bytes=read_bank(argv[2]);const fs::path output=argv[3];fs::create_directories(output);
    auto session=create_resident_replay_session();const auto selection=must(session->inspect_checkpoint_selection(bytes),"original selection");auto installed=session->initialize_checkpoint(catalog,bytes);require(installed.ok,"original bank restore: "+installed.error.original.code);auto exported=must(session->export_state(),"original export");require(!exported.find("task_offer_responses"),"fixture must have response profile disabled");auto prepared=selected_catalog(catalog,selection);require(validate_exported_candidate(exported,prepared).ok,"original native cold roundtrip");
    auto modified=exported;auto* receipts=modified.find("results")->as<V::Dict>();require(receipts&&!receipts->empty(),"actual cached receipts required");StableId result_id;V original_receipt;
    for(auto& entry:*receipts){auto* details=entry.second.find("details");if(details&&details->as<V::Dict>()&&details->as<V::Dict>()->empty()){result_id=boundary::get_string(entry.second,"result_id","receipt");original_receipt=entry.second;*entry.second.find("kind")=V{StableId{"decline_offer"}};break;}}
    require(!result_id.empty(),"actual empty Done receipt required");for(auto& row:*modified.find("history")->as<V::Array>())if(boundary::get_string(row,"result_id","history")==result_id)*row.find("kind")=V{StableId{"decline_offer"}};
    auto seed=must(decode_world_seed(modified,*prepared),"decode counterexample");WorldBundle candidate(prepared,std::move(seed));auto result=candidate.validate();write(output/"counterexample.json",boundary::obj({{"base_receipt",original_receipt},{"changed_result_id",V{result_id}},{"changed_kind",V{StableId{"decline_offer"}}},{"old_world_validate_ok",V{result.ok}}}));
    require(result.ok,"counterexample blocked by unrelated validator: "+result.error.original.code);
#ifdef HANMO_RESPONSE_HISTORY_GATE
    result=validate_task_offer_response_history(candidate.common,false);
    require(!result.ok&&result.error.original.code=="TaskOfferResponseProfileRequired","disabled response history not rejected");
    require(validate_task_offer_response_history(candidate.common,true).ok,"guard silently rejects an enabled profile");
    auto plain=must(decode_world_seed(exported,*prepared),"decode original again");WorldBundle old(prepared,std::move(plain));require(validate_task_offer_response_history(old.common,false).ok,"old legal history changed");
    auto cache_only=candidate.common;cache_only.state().history.clear();require(!validate_task_offer_response_history(cache_only,false).ok,"cached-only response bypassed gate");
    auto history_only=candidate.common;history_only.state().results={};require(!validate_task_offer_response_history(history_only,false).ok,"history-only response bypassed gate");
#else
    require(!result.ok,"original cold validator accepted decline_offer history without its profile");
#endif
    require(must(session->checkpoint(),"original installed bank")==bytes,"counterexample modified installed world");write(output/"result.json",boundary::obj({{"ok",V{true}},{"old_profile_native_world_valid",V{true}},{"new_pure_domain_gate_applied",V{true}},{"business_commands",V{Integer{0}}}}));std::cout<<"task response history: actual old native world counterexample rejected by profile history gate\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
