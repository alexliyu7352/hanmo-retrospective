"""有限新作者边界直接消费原checker与schema，不代完整合包或World。"""
import copy,json,unittest
from pathlib import Path
from content.military_tactic_learning import check_military_tactic_learning
ROOT=Path(__file__).resolve().parents[1]
class MilitaryTacticLearningAuthorTest(unittest.TestCase):
    def setUp(self):
        self.pack=json.loads((ROOT/'content/packs/authored/military_tactic_learning_1_0/content.json').read_text())
        self.index={}
        for table,rows in self.pack.items():
            if isinstance(rows,list):
                self.index[table]={}
                for row in rows:
                    if isinstance(row,dict) and 'id' in row:
                        self.index[table][row['id']]=row;self.index[table]['tactic_watch::'+row['id']]=row
        self.index['texts']={**self.pack['texts'],**{'tactic_watch::'+k:v for k,v in self.pack['texts'].items()}}
    def test_actual_author_and_explicit_sources(self):
        self.assertEqual(check_military_tactic_learning(self.pack,self.index),[])
        policy=self.pack['scenarios'][0]['military_tactic_learning']
        self.assertEqual(len(policy['definitions']),8);self.assertEqual(len(policy['initial_grants']),8)
        self.assertEqual({g['actor_id'] for g in policy['initial_grants']},{'tactic_watch::actor.instructor'})
        self.assertFalse(any(g['actor_id']=='qingxi::actor.player' for g in policy['initial_grants']))
    def test_half_pair_and_bad_authority(self):
        p=copy.deepcopy(self.pack);p['scenarios'][0]['dynamic_world'].pop('growth_tactic_lessons');self.assertTrue(check_military_tactic_learning(p,self.index))
        p=copy.deepcopy(self.pack);p['scenarios'][0]['military_tactic_learning']['initial_grants'][0]['actor_id']='missing';self.assertTrue(check_military_tactic_learning(p,self.index))
        p=copy.deepcopy(self.pack);p['scenarios'][0]['military_tactic_learning']['definitions'][0]['teacher_actor_ids']=['missing'];self.assertTrue(check_military_tactic_learning(p,self.index))
    def test_exact_troop_effect(self):
        index=copy.deepcopy(self.index);index['troop_types']['troop.infantry']['tactics'][0]['morale_effect']=15;self.assertTrue(check_military_tactic_learning(self.pack,index))
    def test_initial_projection_is_authoritative(self):
        index=copy.deepcopy(self.index)
        # 原青溪目录即使仍指旧兵种，实际初态的新模板也必须逐项检查。
        for camp in self.pack['scenarios'][0]['enabled']['garrisons']:
            index.setdefault('garrisons',{})[camp]={'units':[{'id':'old.unit','troop_type_id':'old.troop'}]}
        index['troop_types']['old.troop']={'tactics':[{'id':'unrelated.invalid'}]}
        self.assertEqual(check_military_tactic_learning(self.pack,index),[])
        index['troop_types']['troop.infantry']['tactics'][0]['morale_effect']=15
        self.assertTrue(check_military_tactic_learning(self.pack,index))
    def test_initial_missing_duplicate_and_unknown_troop(self):
        p=copy.deepcopy(self.pack);p['scenarios'][0]['initial']['garrisons'].pop()
        self.assertTrue(check_military_tactic_learning(p,self.index))
        p=copy.deepcopy(self.pack);rows=p['scenarios'][0]['initial']['garrisons'];rows.append(copy.deepcopy(rows[0]))
        self.assertTrue(check_military_tactic_learning(p,self.index))
        p=copy.deepcopy(self.pack);p['scenarios'][0]['initial']['garrisons'][0]['units'][0]['troop_type_id']='missing'
        self.assertTrue(check_military_tactic_learning(p,self.index))
    def test_actual_new_subschema(self):
        import jsonschema
        schema=json.loads((ROOT/'schemas/content.schema.json').read_text());schema['$ref']='#/$defs/scenario/properties/military_tactic_learning';schema.pop('properties',None);schema.pop('required',None);schema.pop('additionalProperties',None)
        selected=self.pack['scenarios'][0]['military_tactic_learning'];jsonschema.validate(selected,schema)
        bad=copy.deepcopy(selected);bad['definitions'][0]['free_xp']=100
        with self.assertRaises(jsonschema.ValidationError):jsonschema.validate(bad,schema)
if __name__=='__main__':unittest.main()
