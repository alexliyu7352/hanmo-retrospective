"""回复声明与原托留目录同配，源门不生成真实领读或答复。"""
import copy
import unittest
from tools.content.visit_reply_links import check_visit_message_replies
class VisitReplyAuthorTest(unittest.TestCase):
    def setUp(self):
        """同一个原接待人只表示可受理托回和领取。"""
        self.policy={'id':'return','message_policy_id':'message','reply_node_id':'reply','collect_node_id':'collect'}
        self.scene={'visit_reply_profile':'witnessed_visit_replies_v1','visit_message_replies':{'version':1,'policies':[self.policy]},'visit_message_profile':'witnessed_reception_v1','visit_messages':{'version':1,'policies':[{'id':'message','custodian_actor_id':'custodian','role_id':'role','dialogue_id':'talk'}]},'dynamic_world':{'profile':'local_society_v1'}}
        self.pack={'required_capabilities':['visit_message_replies_v1','visit_messages_v1','dialogue','multi_actor','native_dynamic_world'],'scenarios':[self.scene]}
        self.index={'roles':{'role':{'initial_actor':'custodian','dialogue_id':'talk'}},'dialogues':{'talk':{'context_role':'role','nodes':[{'id':n,'kind':'say','speaker':'$provider'} for n in ['reply','collect']]}}}
    def test_pure_pair(self):
        """声明不添加任何实例或实际获知。"""
        old=copy.deepcopy((self.pack,self.index));self.assertEqual([],check_visit_message_replies(self.pack,self.index));self.assertEqual(old,(self.pack,self.index));self.assertEqual([],check_visit_message_replies({'scenarios':[{}]},{}))
    def test_required_pair(self):
        """无原消息来源或cap不得降级为任意回帖。"""
        for k in ['visit_reply_profile','visit_message_profile','visit_messages']:
            old=self.scene.pop(k);self.assertTrue(check_visit_message_replies(self.pack,self.index));self.scene[k]=old
        self.pack['required_capabilities'].remove('visit_message_replies_v1');self.assertTrue(check_visit_message_replies(self.pack,self.index))
    def test_exact_policy(self):
        """不接另一场景政策，不把接受预约作为回复目录字段。"""
        self.policy['message_policy_id']='unknown';self.assertTrue(check_visit_message_replies(self.pack,self.index));self.policy['message_policy_id']='message';self.policy['appointment_id']='injected';self.assertTrue(check_visit_message_replies(self.pack,self.index));del self.policy['appointment_id'];self.scene['visit_message_replies']['policies'].append(copy.deepcopy(self.policy));self.assertTrue(check_visit_message_replies(self.pack,self.index))
    def test_actual_say(self):
        """不能靠本人/其他人的说话或action节点代原接待人声明。"""
        node=self.index['dialogues']['talk']['nodes'][0]
        for key,value in [('speaker','$actor'),('kind','action'),('command',{})]:
            old=node.get(key);node[key]=value;self.assertTrue(check_visit_message_replies(self.pack,self.index))
            if old is None:del node[key]
            else:node[key]=old
        self.policy['collect_node_id']='reply';self.assertTrue(check_visit_message_replies(self.pack,self.index))
if __name__=='__main__':unittest.main()
