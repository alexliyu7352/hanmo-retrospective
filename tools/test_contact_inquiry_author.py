"""探听作者严格来源门，不构造运行期见闻或声称正常世界已经到场。"""
import copy
import unittest
from tools.content.contact_inquiry_links import check_contact_inquiries

class ContactInquiryAuthorTest(unittest.TestCase):
    """明确源、披露范围和原会面约束。"""
    def setUp(self):
        """原说话者和对象不同，只有一个私人say节点。"""
        self.policy={'id':'ask','provider_actor_id':'person','subject_actor_id':'friend','role_id':'private','dialogue_id':'talk','node_id':'ask','disclosure':'share_last_seen','duration_ticks':1}
        self.scene={'contact_inquiry_profile':'witnessed_personal_inquiries_v1','contact_inquiries':{'version':1,'policies':[self.policy]},'task_message_handoff_profile':'observed_letter_delivery_v1','dynamic_world':{'profile':'local_society_v1'},'enabled':{'actors':['person','friend'],'roles':['private'],'dialogues':['talk'],'places':['house']}}
        self.pack={'required_capabilities':['contact_inquiries_v1','dialogue','multi_actor','native_dynamic_world'],'scenarios':[self.scene]}
        self.index={'roles':{'private':{'initial_actor':'person','place_id':'house','dialogue_id':'talk','organization_id':None}},'dialogues':{'talk':{'context_role':'private','nodes':[{'id':'ask','kind':'say','speaker':'$provider'}]}}}
    def test_valid_pure(self):
        """只检查声明，不新增知识或人物位置。"""
        before=copy.deepcopy((self.pack,self.index));self.assertEqual([],check_contact_inquiries(self.pack,self.index));self.assertEqual(before,(self.pack,self.index))
    def test_profile_source_and_capability(self):
        """单边启用、无真实旧源或缺声明不能降级成全世界扫描。"""
        self.assertEqual([],check_contact_inquiries({'scenarios':[{}]},{}))
        for key in ['contact_inquiry_profile','task_message_handoff_profile']:
            saved=self.scene.pop(key);self.assertTrue(check_contact_inquiries(self.pack,self.index));self.scene[key]=saved
        self.pack['required_capabilities'].remove('contact_inquiries_v1');self.assertTrue(check_contact_inquiries(self.pack,self.index))
    def test_duration_and_disclosure(self):
        """不能零时探秘密，也不能选择实时坐标或递归传闻。"""
        for key,value in [('duration_ticks',0),('duration_ticks',True),('duration_ticks',2),('disclosure','current_location'),('disclosure','rumor')]:
            saved=self.policy[key];self.policy[key]=value;self.assertTrue(check_contact_inquiries(self.pack,self.index));self.policy[key]=saved
        self.policy['disclosure']='decline';self.assertEqual([],check_contact_inquiries(self.pack,self.index))
    def test_original_person_and_say(self):
        """原私人人格不由岗位接替，旁人/choice不能代答。"""
        role=self.index['roles']['private']
        for key,value in [('initial_actor','deputy'),('organization_id','office'),('place_id','unknown')]:
            saved=role[key];role[key]=value;self.assertTrue(check_contact_inquiries(self.pack,self.index));role[key]=saved
        node=self.index['dialogues']['talk']['nodes'][0]
        for key,value in [('speaker','$actor'),('kind','choice'),('command',{})]:
            saved=node.get(key);node[key]=value;self.assertTrue(check_contact_inquiries(self.pack,self.index))
            if saved is None:del node[key]
            else:node[key]=saved
    def test_subject_and_policy_uniqueness(self):
        """不能问未启用对象或重复政策，也不能把自己装成另一个目标。"""
        self.policy['subject_actor_id']='missing';self.assertTrue(check_contact_inquiries(self.pack,self.index));self.policy['subject_actor_id']='person';self.assertTrue(check_contact_inquiries(self.pack,self.index));self.policy['subject_actor_id']='friend';self.scene['contact_inquiries']['policies'].append(copy.deepcopy(self.policy));self.assertTrue(check_contact_inquiries(self.pack,self.index))

if __name__=='__main__':unittest.main()
