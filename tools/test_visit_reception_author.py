"""只验来访作者来源和路径，无初态消息或运行期造数据。"""
from copy import deepcopy
import unittest
from content.visit_reception_links import check_visit_reception


class VisitReceptionAuthorTest(unittest.TestCase):
    def setUp(self):
        self.intent=dict(id='intent',actor_id='receiver',message_policy_id='message',purpose='receive',cooldown_ticks=4,route=[dict(node_id='greeting'),dict(node_id='menu',option_id='receive')])
        self.scene=dict(visit_reception_profile='authored_local_reception_v1',visit_reception_intents=dict(version=1,intents=[self.intent]),visit_message_profile='witnessed_reception_v1',visit_messages=dict(policies=[dict(id='message',sender_actor_id='sender',recipient_actor_id='receiver',dialogue_id='dialog',deposit_node_id='deposit',receive_node_id='receive')]),enabled=dict(actors=['sender','receiver']),dynamic_world=dict(director=dict(subjects={'receiver':dict(templates=['visit_reception'])})))
        self.pack=dict(required_capabilities=['visit_reception_v1','visit_messages_v1','native_dynamic_world','multi_actor','dialogue'],scenarios=[self.scene]);self.index=dict(dialogues={'dialog':dict(entry='greeting',nodes=[dict(id='greeting',kind='say',next='menu'),dict(id='menu',kind='choice',options=[dict(id='receive',next='receive')]),dict(id='receive',kind='say')])})
    def test_original_route(self):self.assertEqual([],check_visit_reception(self.pack,self.index))
    def test_legacy_absent(self):self.assertEqual([],check_visit_reception(dict(scenarios=[dict()]),{}))
    def test_other_listener(self):self.intent['actor_id']='sender';self.assertTrue(check_visit_reception(self.pack,self.index))
    def test_unknown_choice(self):self.intent['route'][1]['option_id']='secret';self.assertTrue(check_visit_reception(self.pack,self.index))
    def test_action_path(self):self.index['dialogues']['dialog']['nodes'][0]['kind']='action';self.assertTrue(check_visit_reception(self.pack,self.index))
    def test_missing_profile(self):del self.scene['visit_reception_profile'];self.assertTrue(check_visit_reception(self.pack,self.index))
    def test_director_absent(self):self.scene['dynamic_world']['director']['subjects']={};self.assertTrue(check_visit_reception(self.pack,self.index))
    def test_duplicate_actor(self):self.scene['visit_reception_intents']['intents'].append(deepcopy(self.intent));self.assertTrue(check_visit_reception(self.pack,self.index))


if __name__=='__main__':unittest.main()
