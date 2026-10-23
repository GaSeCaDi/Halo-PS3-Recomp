import pathlib,csv,json
r=pathlib.Path(r'C:\H3-PS3');ref=json.loads((r/'research/bitmap-reference.json').read_text());count=0
for m in ref['maps']:
 a=list(csv.DictReader((r/'research/tag-index-reference'/(m['map']+'-bitmaps.csv')).open(encoding='utf-8')))
 b=list(csv.DictReader((r/'dist/tag-browser'/(m['map']+'.map-bitmaps.tsv')).open(encoding='utf-8'),delimiter='\t'))
 assert len(a)==len(b)==m['image_records'],m
 for x,y in zip(a,b):
  assert x==y,(m['map'],x,y)
  count+=1
p=list(csv.DictReader((r/'dist/tag-browser/resource-page.tsv').open(),delimiter='\t'));assert len(p)==1
resource=json.loads((r/'research/resource-page-reference.json').read_text())
for k,v in p[0].items():assert str(resource[k])==v,(k,resource[k],v)
text=(r/'tools/RPCS3/log/TTY.log').read_text(encoding='utf-8-sig');assert 'H3RESOURCE_PASS' in text and 'H3INDEX_EXIT 0' in text
out={'status':'PASS','maps_with_images':len(ref['maps']),'image_records_compared_field_by_field':count,'resource_page_bytes':resource['uncompressed_bytes'],'resource_crc32':resource['page_crc32'],'exit_code':0}
(r/'research/bitmap-validation.json').write_text(json.dumps(out,indent=2));print(json.dumps(out,indent=2))
