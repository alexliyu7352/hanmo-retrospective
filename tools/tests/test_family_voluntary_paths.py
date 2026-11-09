"""生前原作者准入反例；实际checker及旧条款接口，不把源码字符串断言冒行为。"""
import copy
import unittest
from content.family_voluntary_paths import check_family_voluntary_paths
from content.voluntary_succession import check_voluntary_succession

class VoluntaryPathsTest(unittest.TestCase):
    def setUp(self):
        """独立已知私人SAY目录夹具，未构造运行时关系或接受。"""
        self.request = {'id':'request','template_id':'meeting','accept_node_id':'yes','withdraw_node_id':'no','relation_kind':'biological_child','disciple_skill_id':'','terms_version':1,'consent_kind':'successor_accepts_living_voluntary_succession_v1','scope':'player_control_and_selected_personal_assets','personal':{'money':0,'goods':[]}}
        self.scene = {'voluntary_succession':{'profile':'living_accepted_family_paths_v2','request_ids':[]},'family_voluntary_paths':{'version':1,'profile':'living_accepted_family_paths_v1','requests':[self.request]},'family_life':{},'family_lineage':{},'dynamic_world':{'known_personal_dialogues':{'version':1,'templates':[{'id':'meeting','dialogue_id':'talk','listener':{'kind':'installed_actor','source_id':'p'},'provider':{'kind':'installed_actor','source_id':'c'}}]}},'enabled':{'dialogues':['talk']}}
        self.pack = {'required_capabilities':['family','family_life_v1','native_dynamic_world','known_personal_dialogues_v1','family_voluntary_paths_v1'],'scenarios':[self.scene],'dialogues':[{'id':'talk','context_role':'','nodes':[{'id':'yes','kind':'say','speaker':'$provider'},{'id':'no','kind':'say','speaker':'$provider'}]}]}
    def test_explicit_pair(self):
        """新无旧岗位请求合法，旧格式仍不能用空旧请求。"""
        self.assertEqual([],check_family_voluntary_paths(self.pack,{}))
        self.assertEqual([],check_voluntary_succession(self.pack,{}))
        self.scene['voluntary_succession']['profile']='authored_voluntary_succession_v1'
        self.assertTrue(check_family_voluntary_paths(self.pack,{}))
        self.assertTrue(check_voluntary_succession(self.pack,{}))
    def test_known_and_life_required(self):
        """明确生命和本人来源不能被单一新版marker替代。"""
        del self.scene['dynamic_world']['known_personal_dialogues']
        self.assertTrue(check_family_voluntary_paths(self.pack,{}))
    def test_actual_provider_say(self):
        """玩家台词和choice各自拒绝。"""
        node=self.pack['dialogues'][0]['nodes'][0];node['speaker']='$player'
        self.assertTrue(check_family_voluntary_paths(self.pack,{}));node['speaker']='$provider';node['kind']='choice'
        self.assertTrue(check_family_voluntary_paths(self.pack,{}))
    def test_declarations_are_distinct(self):
        """撤回/接受不同，死后条件同意也不兼用。"""
        self.request['withdraw_node_id']='yes';self.assertTrue(check_family_voluntary_paths(self.pack,{}));self.request['withdraw_node_id']='no'
        self.scene['family_collateral_legacy']={'requests':[{'template_id':'meeting','accept_node_id':'yes'}]}
        self.assertTrue(check_family_voluntary_paths(self.pack,{}))
    def test_message_return_declaration_is_distinct(self):
        """回书同句不能兼作换控许可，实际不同句仍可并存。"""
        self.scene['task_message_returns']={'policies':[{'dialogue_id':'talk','node_id':'yes'}]}
        self.assertTrue(any(x[0]=='VOLUNTARY_PATH_DISTINCT_DECLARATION' for x in check_family_voluntary_paths(self.pack,{})))
        self.scene['task_message_returns']['policies'][0]['node_id']='reply'
        self.assertEqual([],check_family_voluntary_paths(self.pack,{}))
    def test_relation_dependencies(self):
        """弟子/旁支必须原账；关系标签自身不能建立来源。"""
        self.request['relation_kind']='accepted_disciple';self.request['disciple_skill_id']='rites'
        self.assertTrue(check_family_voluntary_paths(self.pack,{}));self.scene['family_apprenticeship']={}
        self.assertEqual([],check_family_voluntary_paths(self.pack,{}));self.request['relation_kind']='recognized_collateral';self.request['disciple_skill_id']=''
        self.assertTrue(check_family_voluntary_paths(self.pack,{}))
    def test_personal_asset_scope(self):
        """未知品类、重复/负量以及扩大公权范围全部拒。"""
        self.request['personal']['goods']=[{'item_id':'grain','quantity':1}]
        self.assertTrue(check_family_voluntary_paths(self.pack,{}));self.pack['items']=[{'id':'grain'}]
        self.assertEqual([],check_family_voluntary_paths(self.pack,{}));self.request['personal']['goods'][0]['quantity']=-1
        self.assertTrue(check_family_voluntary_paths(self.pack,{}));self.request['personal']={'money':0,'goods':[]};self.request['scope']='all_estate'
        self.assertTrue(check_family_voluntary_paths(self.pack,{}))
    def test_false_version_and_extra(self):
        """布尔不代版本，额外许可字段不被吞掉。"""
        self.scene['family_voluntary_paths']['version']=True;self.assertTrue(check_family_voluntary_paths(self.pack,{}))
        self.scene['family_voluntary_paths']['version']=1;self.request['authorization']='implicit';self.assertTrue(check_family_voluntary_paths(self.pack,{}))

if __name__ == '__main__': unittest.main()
