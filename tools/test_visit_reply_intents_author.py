"""真实回复1.1作者与拒绝门，不开素材导出或伪造运行世界。"""
import copy,json,unittest
from pathlib import Path
from tools.content.visit_reply_links import check_visit_message_replies,check_visit_reply_intents
ROOT=Path(__file__).resolve().parents[1]
class VisitReplyIntentAuthorTest(unittest.TestCase):
    def setUp(self):
        """每门都从冻结作者输入读取，变坏副本不改真实包。"""
        self.pack=json.loads((ROOT/'content/packs/authored/visit_replies_1_1/content.json').read_text());self.index={k:{x['id']:x for x in v} for k,v in self.pack.items() if isinstance(v,list) and all(isinstance(x,dict) and 'id' in x for x in v)};self.scene=self.pack['scenarios'][0];self.intent=self.scene['visit_reply_intents']['intents'][0]
    def test_original_assets_and_declared_nodes(self):
        """新意向原值合法，角色/钱货/对白不为回复新增捷径。"""
        self.assertEqual([],check_visit_message_replies(self.pack,self.index));self.assertEqual([],check_visit_reply_intents(self.pack,self.index));old=json.loads((ROOT/'content/packs/authored/visit_replies_1_0/content.json').read_text())
        for key in ['actors','wallets','inventories','roles','dialogues','offers','relations','estates','items']:self.assertEqual(old[key],self.pack[key])
    def test_wrong_reply_or_original_source(self):
        """不许把拜帖回复变成接受预约，也不能假接另一人原来访。"""
        for key,value in [('response','accepted_appointment'),('reception_intent_id','unknown'),('reply_policy_id','unknown')]:
            old=self.intent[key];self.intent[key]=value;self.assertTrue(check_visit_reply_intents(self.pack,self.index));self.intent[key]=old
    def test_exact_route_and_no_command(self):
        """真实receive节点至reply节点必须有原有限路径且无自动业务。"""
        self.intent['route'][0]['node_id']='withdraw';self.assertTrue(check_visit_reply_intents(self.pack,self.index));self.intent['route'][0]['node_id']='receive';d=self.index['dialogues']['dialog.visit_reception'];n=next(n for n in d['nodes'] if n['id']=='receive');n['command']={};self.assertTrue(check_visit_reply_intents(self.pack,self.index))
    def test_version_pair_and_cap(self):
        """原v1不能悄悄选NPC意向；新段必须明确能力。"""
        self.scene['visit_reply_profile']='witnessed_visit_replies_v1';self.assertTrue(check_visit_reply_intents(self.pack,self.index));self.scene['visit_reply_profile']='witnessed_authored_visit_replies_v2';self.pack['required_capabilities'].remove('visit_message_replies_v2');self.assertTrue(check_visit_reply_intents(self.pack,self.index))
if __name__=='__main__':unittest.main()
