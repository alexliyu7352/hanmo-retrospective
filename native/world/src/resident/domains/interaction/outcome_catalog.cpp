// 结果回访目录只声明原采购模板及只读对白；本文件不增加任务、回执或任意业务动作。
#include "regional_outcome_dialogue.hpp"
#include "resident/domains/tasks/task_model.hpp"
#include "resident/storage/common_codec.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
namespace {
// 限定作者身份的读取长度，空值不能退化成匹配所有模板。
bool valid_id(const StableId& value) { return !value.empty() && value.size() <= 4096; }
// 只找已经解码的同一对白定义，未声明的图节点不由回访目录补造。
const DialogueDefinition* find_dialogue(const InteractionCatalog& catalog,const StableId& id) {
    for(const auto& row:catalog.dialogues) if(row.id==id) return &row;
    return nullptr;
}
}
Status decode_regional_outcome_catalog(const BoundaryValue& value,InteractionCatalog& catalog,const TaskCatalog& tasks) {
    try {
        using namespace boundary;
        if(!catalog.regional_outcome_topics.empty()) return common_failure("DuplicateRegionalOutcomeCatalog");
        if(!catalog.common) return common_failure("RegionalOutcomeCatalogUnavailable");
        std::set<StableId> ids,dialogues;
        for(const auto& topic:catalog.regional_actor_topics) { ids.insert(topic.id);dialogues.insert(topic.dialogue_id); }
        for(const auto& topic:catalog.regional_need_topics) { ids.insert(topic.id);dialogues.insert(topic.dialogue_id); }
        std::vector<RegionalOutcomeTopic> topics;
        for(const auto& row:array(value,"regional_life/outcome_topics")) {
            fields(row,{"id","role_id","task_template_id","dialogue_id","cooldown_ticks"},{},"outcome_topic");
            RegionalOutcomeTopic topic{get_string(row,"id","outcome_topic"),get_string(row,"role_id","outcome_topic"),
                get_string(row,"task_template_id","outcome_topic"),get_string(row,"dialogue_id","outcome_topic"),get_int(row,"cooldown_ticks","outcome_topic")};
            const auto* role=catalog.common->roles.find(topic.role_id);
            const auto* task=tasks.templates.find(topic.task_template_id);
            const auto* dialogue=find_dialogue(catalog,topic.dialogue_id);
            if(!valid_id(topic.id)||!ids.insert(topic.id).second||!dialogues.insert(topic.dialogue_id).second||
                !role||!task||task->template_id!="procurement"||task->report_role!=topic.role_id||
                !dialogue||dialogue->context_role!=topic.role_id||topic.cooldown_ticks<0||topic.cooldown_ticks>720||
                std::find(role->services.begin(),role->services.end(),"report_offer")==role->services.end())
                return common_failure("InvalidRegionalOutcomeTopic",topic.id);
            const Owner expected=role->organization_id?Owner{Owner::Kind::organization,*role->organization_id,{},{}}:
                Owner{Owner::Kind::actor,role->initial_actor,{},{}};
            if(!owner_equal(expected,task->issuer)) return common_failure("RegionalOutcomeIssuerMismatch",topic.id);
            for(const auto& node:dialogue->nodes) {
                if((node.kind!="say"&&node.kind!="choice"&&node.kind!="end")||node.command||node.condition)
                    return common_failure("RegionalOutcomeCommandDenied",topic.id);
                if(node.kind=="say"&&node.speaker!="$provider"&&node.speaker!="$player")
                    return common_failure("InvalidRegionalSpeaker",topic.id);
            }
            topics.push_back(std::move(topic));
            if(ids.size()>64) return common_failure("RegionalTopicLimit");
        }
        catalog.regional_outcome_topics=std::move(topics);
        return common_success();
    } catch(const boundary::DecodeFailure& error) { return {false,{},error.error}; }
}
}
