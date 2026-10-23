import pathlib,json,csv
r=pathlib.Path(r'C:\H3-PS3');refs=json.loads((r/'research/tag-index-reference/maps-index-reference.json').read_text());results=list(csv.DictReader((r/'dist/tag-browser/tag-index-results.tsv').open(),delimiter='\t'))
assert len(results)==len(refs)==25
checked=0
for actual in results:
 ref=next(x for x in refs if x['map']+'.map'==actual['map'])
 assert int(actual['tags'])==ref['tag_count']
 assert actual['index_crc32']==ref['index_crc32']
 assert actual['scenario_crc32']==ref['scenario_prefix_crc32']
 if not ref['tag_count']:assert actual['status']=='EMPTY';continue
 for k,rk in [('groups','group_count'),('nulls','null_count'),('globals','global_count')]:assert int(actual[k])==ref[rk]
 host=list(csv.DictReader((r/'research/tag-index-reference'/(ref['map']+'-tags.csv')).open(encoding='utf-8')))
 ps3=list(csv.DictReader((r/'dist/tag-browser'/(actual['map']+'-tags.tsv')).open(encoding='utf-8'),delimiter='\t'))
 assert len(host)==len(ps3)==ref['tag_count']
 for a,b in zip(host,ps3):
  assert a['index']==b['index'] and a['id']==b['id'] and a['name']==b['name'],(ref['map'],a['index'],'identity')
  assert (a['group_hex'] or '00000000')==b['group_hex']
  assert int(b['file_offset'],16)==(int(a['file_offset']) if a['file_offset'] else 0)
  assert b['valid']==('0' if a['group_index']=='65535' else '1')
  checked+=1
text=(r/'logs/tag-browser-TTY.log').read_text(encoding='utf-8-sig');assert 'checks: 15/15' in text and 'H3INDEX_EXIT 0' in text
out={'status':'PASS','maps':25,'nonempty_maps':23,'entries_compared_field_by_field':checked,'crc_matches':25,'negative_and_positive_checks':'15/15','exit_code':0}
(r/'research/tag-index-validation.json').write_text(json.dumps(out,indent=2));print(json.dumps(out,indent=2))
