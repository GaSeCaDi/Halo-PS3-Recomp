import pathlib,csv,json,struct,collections,zlib
r=pathlib.Path(r'C:\H3-PS3');refs=json.loads((r/'research/tag-index-reference/maps-index-reference.json').read_text());report=[];total=0
for m in refs:
 if not m['tag_count']:continue
 tags=list(csv.DictReader((r/'research/tag-index-reference'/(m['map']+'-tags.csv')).open(encoding='utf-8')))
 bias=int(m['virtual_base'],16)-m['sections'][2]['start'];rows=[];empty=0;bitmaps=0
 with (r/'HALO 3/maps'/(m['map']+'.map')).open('rb') as f:
  def read(off,n):
   if off<m['sections'][2]['start'] or off+n>m['sections'][2]['start']+m['sections'][2]['size']:raise ValueError('tag block outside section')
   f.seek(off);b=f.read(n);assert len(b)==n;return b
  for tag in tags:
   if tag['group']!='bitm':continue
   bitmaps+=1;b=read(int(tag['file_offset']),0xa4)
   normal,nva,_=struct.unpack_from('>3I',b,0x8c);interleaved,iva,_=struct.unpack_from('>3I',b,0x98)
   handles=read(nva-bias,normal*8) if normal else b''
   if interleaved:read(iva-bias,interleaved*8)
   has_images=False
   for bank,blockoff in [(0,0x60),(1,0x80)]:
    count,va,_=struct.unpack_from('>3I',b,blockoff)
    assert count<=4096
    if not count:continue
    has_images=True;images=read(va-bias,count*48)
    for i in range(count):
     d=images[i*48:(i+1)*48];assert d[:4]==b'bitm';w,h=struct.unpack_from('>HH',d,4);assert w and h
     row={'tag_index':tag['index'],'tag_id':tag['id'],'image_set':bank,'image_index':i,'width':w,'height':h,'depth':d[8],
          'xbox_flags':d[9],'type':struct.unpack_from('>H',d,10)[0],'format':struct.unpack_from('>H',d,12)[0],
          'flags':struct.unpack_from('>H',d,14)[0],'mips':d[20],'resources':normal,'interleaved':interleaved,
          'resource_id':f'{struct.unpack_from(">I",handles,i*8)[0]:08x}' if i<normal else 'ffffffff','name':tag['name']}
     rows.append(row)
   if not has_images:empty+=1
 with (r/'research/tag-index-reference'/(m['map']+'-bitmaps.csv')).open('w',encoding='utf-8',newline='') as out:
  if rows:w=csv.DictWriter(out,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
 report.append({'map':m['map'],'bitmap_tags':bitmaps,'image_records':len(rows),'empty_bitmap_tags':empty});total+=len(rows)
 print(m['map'],'bitmaps',bitmaps,'images',len(rows),'empty',empty,flush=True)
(r/'research/bitmap-reference.json').write_text(json.dumps({'image_records':total,'maps':report},indent=2))
# First non-interleaved bitmap in mainmenu points to resource datum e1630000.
m=next(x for x in refs if x['map']=='mainmenu');tags=list(csv.DictReader((r/'research/tag-index-reference/mainmenu-tags.csv').open()));bias=int(m['virtual_base'],16)-m['sections'][2]['start']
f=(r/'HALO 3/maps/mainmenu.map').open('rb')
def rd(off,n):f.seek(off);b=f.read(n);assert len(b)==n;return b
zone=rd(int(next(t for t in tags if t['group']=='zone')['file_offset']),0x64);play=rd(int(next(t for t in tags if t['group']=='play')['file_offset']),60)
resources,rv,_=struct.unpack_from('>3I',zone,0x58);d=rd(rv-bias,64);seg=struct.unpack_from('>h',d,0x22)[0];sections,sv,_=struct.unpack_from('>3I',play,0x30);s=rd(sv-bias+seg*16,16);pi,optional,segmentoff,optoff,_,_=struct.unpack('>hhii hh',s)
pages,pv,_=struct.unpack_from('>3I',play,0x18);p=rd(pv-bias+pi*88,88);block,compressed,uncompressed=struct.unpack_from('>3I',p,8);raw=rd(m['sections'][1]['delta']+block,compressed);obj=zlib.decompressobj(-15);decoded=obj.decompress(raw)+obj.flush();assert len(decoded)==uncompressed and not obj.unused_data
result={'resource_id':'e1630000','source_map':'mainmenu.map','page_index':pi,'codec':p[3],'compressed_bytes':compressed,'uncompressed_bytes':uncompressed,'segment_offset':segmentoff,'compressed_crc32':f'{zlib.crc32(raw):08x}','page_crc32':f'{zlib.crc32(decoded):08x}','segment_prefix_crc32':f'{zlib.crc32(decoded[segmentoff:segmentoff+64]):08x}'}
(r/'research/resource-page-reference.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
