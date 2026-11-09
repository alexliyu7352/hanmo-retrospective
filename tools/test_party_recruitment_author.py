"""正常作者准入及说话者风险门，不创建NPC会话、同意或初始成员。"""
from copy import deepcopy
import unittest
from content.party_recruitment_links import check_party_recruitment_consents


class PartyRecruitmentAuthorTest(unittest.TestCase):
    """同一个原岗位只有真正provider声明能赋予新招伴源。"""
    def setUp(self):
        self.declaration = dict(id='willing', leader_actor_id='leader', companion_actor_id='companion', role_id='guide', dialogue_id='talk', node_id='reply', decision='accept')
        self.pack = {'required_capabilities': ['party_provider_consent_v1','native_dynamic_world','multi_actor','dialogue','companions'], 'scenarios':[{'dynamic_world':{'profile':'local_society_v1'},'party_recruitment_profile':'provider_declaration_v1','party_recruitment_consents':{'version':1,'declarations':[self.declaration]},'enabled':{'actors':['leader','companion'],'roles':['guide'],'dialogues':['talk']}}]}
        self.index = {'roles':{'guide':{'initial_actor':'companion','services':['join_party']}},'dialogues':{'talk':{'context_role':'guide','nodes':[{'id':'reply','kind':'say','speaker':'$provider'}]}}}

    def test_valid_and_decline(self):
        """接受和拒绝均是原本人声明，验证不会把拒绝变为同意。"""
        before=deepcopy(self.pack);self.assertEqual([],check_party_recruitment_consents(self.pack,self.index));self.assertEqual(before,self.pack)
        self.declaration['decision']='decline';self.assertEqual([],check_party_recruitment_consents(self.pack,self.index))

    def test_false_speaker_and_choice(self):
        """领队/第三人/仅到达选项不能代NPC答应。"""
        node=self.index['dialogues']['talk']['nodes'][0]
        for speaker in ('$player','someone_else'):
            node['speaker']=speaker;self.assertTrue(check_party_recruitment_consents(self.pack,self.index))
        node.update(speaker='$provider',kind='choice');self.assertTrue(check_party_recruitment_consents(self.pack,self.index))

    def test_exact_profile_and_direction(self):
        """能力、profile和邀请方向缺一就拒；旧情景不被能力并集启用。"""
        scene=self.pack['scenarios'][0];original=deepcopy(scene)
        scene.pop('party_recruitment_profile');self.assertTrue(check_party_recruitment_consents(self.pack,self.index));scene.pop('party_recruitment_consents');self.assertEqual([],check_party_recruitment_consents(self.pack,self.index))
        self.pack['scenarios'][0]=original;self.pack['required_capabilities'].remove('party_provider_consent_v1');self.assertTrue(check_party_recruitment_consents(self.pack,self.index))
        self.pack['required_capabilities'].append('party_provider_consent_v1');original['party_invitations']={'invitations':[{'role_id':'guide'}]};self.assertTrue(check_party_recruitment_consents(self.pack,self.index))

    def test_unknown_or_duplicate_actor_node(self):
        """完整引用必须在原情景启用，不能藏远方人物或第二节点意愿。"""
        scene=self.pack['scenarios'][0];scene['enabled']['actors'].remove('companion');self.assertTrue(check_party_recruitment_consents(self.pack,self.index));scene['enabled']['actors'].append('companion')
        scene['party_recruitment_consents']['declarations'].append(deepcopy(self.declaration));self.assertTrue(check_party_recruitment_consents(self.pack,self.index))


if __name__=='__main__':unittest.main()
