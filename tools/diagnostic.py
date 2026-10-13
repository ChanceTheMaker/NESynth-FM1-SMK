"""Original NROM diagnostic: text, movable sprite, pulse tone, color button."""
from pathlib import Path

FONT={
'A':['01110','10001','10001','11111','10001','10001','10001'],
'B':['11110','10001','10001','11110','10001','10001','11110'],
'C':['01111','10000','10000','10000','10000','10000','01111'],
'D':['11110','10001','10001','10001','10001','10001','11110'],
'E':['11111','10000','10000','11110','10000','10000','11111'],
'F':['11111','10000','10000','11110','10000','10000','10000'],
'G':['01111','10000','10000','10111','10001','10001','01111'],
'H':['10001','10001','10001','11111','10001','10001','10001'],
'I':['11111','00100','00100','00100','00100','00100','11111'],
'K':['10001','10010','10100','11000','10100','10010','10001'],
'L':['10000','10000','10000','10000','10000','10000','11111'],
'M':['10001','11011','10101','10101','10001','10001','10001'],
'N':['10001','11001','10101','10011','10001','10001','10001'],
'O':['01110','10001','10001','10001','10001','10001','01110'],
'P':['11110','10001','10001','11110','10000','10000','10000'],
'R':['11110','10001','10001','11110','10100','10010','10001'],
'S':['01111','10000','10000','01110','00001','00001','11110'],
'T':['11111','00100','00100','00100','00100','00100','00100'],
'U':['10001','10001','10001','10001','10001','10001','01110'],
'V':['10001','10001','10001','10001','10001','01010','00100'],
'W':['10001','10001','10001','10101','10101','10101','01010'],
'Y':['10001','10001','01010','00100','00100','00100','00100'],
'0':['01110','10001','10011','10101','11001','10001','01110'],
'1':['00100','01100','00100','00100','00100','00100','01110'],
'-':['00000','00000','00000','11111','00000','00000','00000'],
}
class Asm:
    def __init__(self):self.code=bytearray();self.labels={};self.fix=[]
    def emit(self,*values):self.code.extend(values)
    def label(self,name):self.labels[name]=0x8000+len(self.code)
    def abs(self,op,target):
        self.emit(op,0,0);self.fix.append((len(self.code)-2,target,False))
    def branch(self,op,target):
        self.emit(op,0);self.fix.append((len(self.code)-1,target,True))
    def done(self):
        for off,target,relative in self.fix:
            v=self.labels[target]
            if relative:
                v-=0x8000+off+1
                assert -128<=v<=127
                self.code[off]=v&255
            else:self.code[off:off+2]=v.to_bytes(2,'little')
        return self.code

def make_rom():
    a=Asm();a.label('reset');a.emit(0x78,0xd8,0xa2,0xff,0x9a,0xe8,0x8e,0,0x20,0x8e,1,0x20)
    a.label('wait');a.emit(0x2c,2,0x20);a.branch(0x10,'wait')
    a.emit(0xa9,0,0xa2,0);a.label('clear');a.emit(0x95,0,0xe8);a.branch(0xd0,'clear')
    a.emit(0xa9,0xff,0xa2,0);a.label('sprites');a.emit(0x9d,0,2,0xe8);a.branch(0xd0,'sprites')
    a.emit(0xa9,112,0x8d,0,2,0xa9,1,0x8d,1,2,0xa9,0,0x8d,2,2,0xa9,120,0x8d,3,2)
    a.emit(0xa9,0x3f,0x8d,6,0x20,0xa9,0,0x8d,6,0x20,0xa2,0)
    a.label('pal');a.abs(0xbd,'palette');a.emit(0x8d,7,0x20,0xe8,0xe0,32);a.branch(0xd0,'pal')
    a.emit(0xa9,0x20,0x8d,6,0x20,0xa9,0,0x8d,6,0x20)
    for page in range(4):
        a.emit(0xa2,0);a.label(f'nt{page}');a.abs(0xbd,f'page{page}');a.emit(0x8d,7,0x20,0xe8);a.branch(0xd0,f'nt{page}')
    a.emit(0xa9,0x40,0x8d,0x17,0x40,0xa9,1,0x8d,0x15,0x40,
           0xa9,0xb4,0x8d,0,0x40,0xa9,0xfd,0x8d,2,0x40,0xa9,0x08,0x8d,3,0x40)
    a.emit(0xa9,0,0x8d,5,0x20,0x8d,5,0x20,0xa9,0x80,0x8d,0,0x20,0xa9,0x1e,0x8d,1,0x20)
    a.label('loop');a.abs(0x4c,'loop')
    a.label('nmi');a.emit(0x48,0x8a,0x48,0x98,0x48,0xe6,0)
    a.emit(0xa9,1,0x8d,0x16,0x40,0xa9,0,0x8d,0x16,0x40,0xa2,0)
    a.label('read');a.emit(0xad,0x16,0x40,0x29,1,0x95,0x10,0xe8,0xe0,8);a.branch(0xd0,'read')
    for bit,address,op in [(6,0x0203,0xce),(7,0x0203,0xee),(4,0x0200,0xce),(5,0x0200,0xee)]:
        a.emit(0xa5,0x10+bit);a.branch(0xf0,f'no{bit}');a.emit(op,address&255,address>>8);a.label(f'no{bit}')
    # A changes pulse frequency; B changes the background palette.
    a.emit(0xa5,0x10);a.branch(0xf0,'low');a.emit(0xa9,0x7e);a.abs(0x4c,'freq')
    a.label('low');a.emit(0xa9,0xfd);a.label('freq');a.emit(0x8d,2,0x40)
    a.emit(0x2c,2,0x20,0xa9,0x3f,0x8d,6,0x20,0xa9,0,0x8d,6,0x20,
           0xa5,0x11);a.branch(0xf0,'dark');a.emit(0xa9,0x11);a.abs(0x4c,'color')
    a.label('dark');a.emit(0xa9,0x0f);a.label('color');a.emit(0x8d,7,0x20)
    a.emit(0xa9,0x80,0x8d,0,0x20,0xa9,0,0x8d,5,0x20,0x8d,5,0x20,0xa9,2,0x8d,0x14,0x40,0x68,0xa8,0x68,0xaa,0x68,0x40)
    a.label('irq');a.emit(0x40)
    a.label('palette');a.emit(*([0x0f,0x30,0x21,0x16]*4+[0x0f,0x27,0x30,0x16]*4))
    chrdata=bytearray(8192);chrdata[16:24]=bytes([0x18,0x3c,0x7e,0xff,0xff,0x7e,0x3c,0x18])
    tiles={}
    for i,(letter,rows) in enumerate(FONT.items(),2):
        tiles[letter]=i;chrdata[i*16:i*16+7]=bytes(int(row,2)<<2 for row in rows)
    nt=bytearray(1024)
    for row,msg in [(4,'NES ON FM-1'),(7,'NROM DIAGNOSTIC'),(20,'D-PAD MOVE'),(23,'A TONE  B COLOR'),(26,'ORIGINAL TEST CARTRIDGE')]:
        col=(32-len(msg))//2
        for i,ch in enumerate(msg):nt[row*32+col+i]=tiles.get(ch,0)
    for p in range(4):a.label(f'page{p}');a.emit(*nt[p*256:(p+1)*256])
    prg=a.done();assert len(prg)<0x3ffa;prg.extend(b'\xea'*(16384-len(prg)))
    for i,label in enumerate(['nmi','reset','irq']):prg[0x3ffa+2*i:0x3ffc+2*i]=a.labels[label].to_bytes(2,'little')
    return b'NES\x1a'+bytes([1,1,0,0,0,0,0,0,0,0,0,0])+prg+chrdata

if __name__=='__main__':
    dest=Path(__file__).resolve().parents[1]/'web/diagnostic.nes';dest.parent.mkdir(exist_ok=True);dest.write_bytes(make_rom())
