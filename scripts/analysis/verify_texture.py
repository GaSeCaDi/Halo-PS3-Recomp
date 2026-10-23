# SPDX-License-Identifier: GPL-3.0-or-later
# Inverse Xenos coordinates based on public TagTool XboxGraphics.cs.
import pathlib,csv,json,struct,zlib,binascii
r=pathlib.Path(r'C:\H3-PS3');m=next(x for x in json.loads((r/'research/tag-index-reference/maps-index-reference.json').read_text()) if x['map']=='mainmenu');tags=list(csv.DictReader((r/'research/tag-index-reference/mainmenu-tags.csv').open()));bias=int(m['virtual_base'],16)-m['sections'][2]['start']
with (r/'HALO 3/maps/mainmenu.map').open('rb') as f:
 def rd(o,n):
  assert o>=0 and o+n<=m['file_size'];f.seek(o);b=f.read(n);assert len(b)==n;return b
 def u(b,o):return struct.unpack_from('>I',b,o)[0]
 def tag(group):return rd(int(next(t for t in tags if t['group']==group)['file_offset']),532 if group=='zone' else 60)
 z=tag('zone');layout=tag('play');t=next(t for t in tags if t['name'].endswith('\\locked_ui') and t['group']=='bitm');bitmap=rd(int(t['file_offset']),164);datum=u(rd(u(bitmap,0x90)-bias,8),0);resource=rd(u(z,0x5c)-bias+(datum&65535)*64,64)
 definition=rd(u(z,0x148)-bias+u(resource,0x14),u(resource,0x18));w,h=struct.unpack_from('>HH',definition,40);assert definition[47]==0
 section=rd(u(layout,0x34)-bias+struct.unpack_from('>h',resource,34)[0]*16,16);pi=struct.unpack_from('>h',section)[0];segment=u(section,4);page=rd(u(layout,0x1c)-bias+pi*88,88);assert struct.unpack_from('>h',page,4)[0]==0 and page[3]==0
 raw=rd(m['sections'][1]['delta']+u(page,8),u(page,12));decoded=zlib.decompress(raw,-15);assert len(decoded)==u(page,16)
 fix=rd(u(resource,44)-bias,u(resource,40)*8);ptr=next(address for field,address in struct.iter_unpack('>II',fix) if field==12);assert ptr>>29==2
 data=decoded[segment+(ptr&0x1fffffff):segment+(ptr&0x1fffffff)+u(definition,0)]
# Map source blocks to destination with inverse mapping, unlike C forward lookup.
bw=(w+3)//4;bh=(h+3)//4;aw=(bw+31)&~31;out=bytearray(w*h*4);seen=set()
for off in range(0,len(data)-7,8):
 ob=off;ot=((ob&~4095)>>3)+((ob&1792)>>2)+(ob&63);om=ot>>10
 x=(((om%(aw>>5))<<2)+((((ot>>8)&2)+(ob>>6))&3))*8+((((ot>>1)&~15)+(ot&15))&63)//8
 y=(((om//(aw>>5))<<2)+((ot>>9)&1)+((ob&2048)>>10))*8+((((ot&480)+((ot&15)<<1))>>6)&~1)+((ot&16)>>4)
 if x>=bw or y>=bh:continue
 assert (x,y) not in seen;seen.add((x,y));b=bytes(data[off+(i^1)] for i in range(8));a,c,bits=struct.unpack('<HHI',b)
 def rgb(v):return (((v>>11)&31)*255//31,((v>>5)&63)*255//63,(v&31)*255//31)
 # Match bit replication specified for RGB565 rather than rounded scaling.
 def rgb(v):
  rr=(v>>11)&31;g=(v>>5)&63;bb=v&31;return (rr*8+rr//4,g*4+g//16,bb*8+bb//4)
 colors=[(*rgb(a),255),(*rgb(c),255)]
 colors.extend([tuple((2*colors[0][i]+colors[1][i])//3 for i in range(3))+(255,),tuple((colors[0][i]+2*colors[1][i])//3 for i in range(3))+(255,)] if a>c else [tuple((colors[0][i]+colors[1][i])//2 for i in range(3))+(255,),(0,0,0,0)])
 for py in range(4):
  for px in range(4):
   xx=x*4+px;yy=y*4+py
   if xx<w and yy<h:
    rr,g,bb,alpha=colors[(bits>>(2*(py*4+px)))&3];idx=(yy*w+xx)*4;out[idx:idx+4]=bytes((alpha,rr,g,bb))
assert len(seen)==bw*bh
actual=(r/'dist/texture-viewer/texture-argb.bin').read_bytes();assert actual==out,'Pixel mismatch'
# PNG with opaque checkerboard alpha compositing, no external packages.
def chunk(kind,b):return struct.pack('>I',len(b))+kind+b+struct.pack('>I',binascii.crc32(kind+b)&0xffffffff)
pngdata=bytearray()
for y in range(h):
 pngdata.append(0)
 for x in range(w):
  a,rr,g,bb=out[(y*w+x)*4:(y*w+x+1)*4];bg=64 if ((x//16)^(y//16))&1 else 96
  pngdata.extend((v*a+bg*(255-a))//255 for v in (rr,g,bb))
(r/'research/locked-ui-decoded.png').write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(bytes(pngdata)))+chunk(b'IEND',b''))
report={'status':'PASS','texture':t['name'],'resource_id':f'{datum:08x}','width':w,'height':h,'pixels_compared':w*h,'bytes_compared':len(out),'pixel_crc32':f'{zlib.crc32(out):08x}','page_crc32':f'{zlib.crc32(decoded):08x}','blocks_compared':len(seen)}
(r/'research/texture-validation.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
