import struct
import os

WIDTH = 240
HEIGHT = 400
START_FRAME = 0
FPS = 30
SECTOR_SIZE = 0x200

AUDIO_FREQ = 22050
AUDIO_CHANNELS = 1
AUDIO_SIZE_BYTES = (AUDIO_FREQ * 2 * AUDIO_CHANNELS // FPS)

TOTAL_SECTOR_OFFSET=68
HEADER_BYTE=110

frames = 0
frame_sectors=[]

def main():
    size3 = WIDTH*HEIGHT*3
    size2 = WIDTH*HEIGHT*2
    file = open("movie.rgb","rb")
    file_audio = open("output.raw","rb")
    file_out = open(r"test.bin","wb")

    file.seek(0,os.SEEK_END)
    frames = file.tell()//size3
    
    file.seek(START_FRAME * size3, os.SEEK_SET)

    for i in range(frames-1):
        image = file.read(size3)
        file_audio.seek((AUDIO_SIZE_BYTES//2)*i, os.SEEK_SET)
        audio = file_audio.read((AUDIO_SIZE_BYTES//2))

        sector_num=file_out.tell()//SECTOR_SIZE
        frame_sectors.append(sector_num)

        #header
        b = make_header_data(sector_num,i)
        file_out.write(b)
        sector_offset = len(b)
        pad_sector(sector_offset,file_out)
        #audio(16bit->8bit)
        for level in range(AUDIO_SIZE_BYTES//2):
            tmpu8 = int.from_bytes(audio[level:level+1], 'little', signed=False)
            tmpu7  = tmpu8>>1
            tmpcmd = (127-tmpu7)<<7 | tmpu7
            #print(level,",",tmps16,",",tmpu16,",",tmpu7,",",tmpcmd)
            file_out.write(struct.pack("H",tmpcmd))
        pad_sector(AUDIO_SIZE_BYTES & 0x1ff,file_out)
        #image(24bit->16bit)
        for pix in range(WIDTH*HEIGHT):
            r=image[3*pix+0]>>3
            g=image[3*pix+1]>>2
            b=image[3*pix+2]>>3
            data = r<<11|g<<5|b
            datal = data & 0xFF
            datah = (data>>8) & 0xFF
            data = (datal<<8)|datah
            file_out.write(struct.pack("H",data))
        pad_sector(size2 & 0x1ff,file_out)
    
    total_sectors=file_out.tell()//SECTOR_SIZE

    for i in range(frames-1):
        file_out.seek(SECTOR_SIZE*frame_sectors[i]+TOTAL_SECTOR_OFFSET, os.SEEK_SET)
        file_out.write(struct.pack("I",total_sectors))
        v=frame_sectors[frames-1-1]
        file_out.write(struct.pack("I",v))
        
        v=frame_sectors[i+1] if (i<frames-1-1) else 0xffffffff
        file_out.write(struct.pack("I",v))
        v=frame_sectors[i+2] if (i<frames-1-2) else 0xffffffff
        file_out.write(struct.pack("I",v))
        v=frame_sectors[i+4] if (i<frames-1-4) else 0xffffffff
        file_out.write(struct.pack("I",v))
        v=frame_sectors[i+8] if (i<frames-1-8) else 0xffffffff
        file_out.write(struct.pack("I",v))

        v=frame_sectors[i-1] if (i>=1) else 0xffffffff
        file_out.write(struct.pack("I",v))
        v=frame_sectors[i-2] if (i>=2) else 0xffffffff
        file_out.write(struct.pack("I",v))
        v=frame_sectors[i-4] if (i>=4) else 0xffffffff
        file_out.write(struct.pack("I",v))
        v=frame_sectors[i-8] if (i>=8) else 0xffffffff
        file_out.write(struct.pack("I",v))

    file_out.close()


def make_header_data(sec_num,fra_num):
    mark0=0xffffffff    #I
    mark1=0xffffffff    #I
    magic='PLAT'.encode()#4s
    major=0             #B
    minior=60           #B
    debug=0             #B
    spare=0             #B

    sector_number=sec_num     #I
    frame_number=fra_num      #I
    hh=to_bcd(((fra_num+START_FRAME)//(60*60*FPS))    )             #B
    mm=to_bcd(((fra_num+START_FRAME)//(   60*FPS))%60 )             #B
    ss=to_bcd(((fra_num+START_FRAME)//(      FPS))%60 )             #B
    ff=to_bcd( (fra_num+START_FRAME)              %FPS)             #B
    header_words=((HEADER_BYTE+(HEIGHT+1)+1)+3)//4   #I

    width=WIDTH           #H
    height=HEIGHT          #H
    image_words=WIDTH*HEIGHT*2//4       #I 16bit
    audio_words=AUDIO_SIZE_BYTES//4#I 16bitcmd
    audio_freq=AUDIO_FREQ#I

    audio_channels=AUDIO_CHANNELS   #B
    pad0=0                          #B
    pad1=0                          #B
    pad2=0                          #B
    unused0=0                       #I
    unused1=0                       #I
    unused2=0                       #I

    unused2=0                       #I
    total_sectors=0                 #I
    last_sector=0                   #I
    forward_frame_sector0=0          #I

    forward_frame_sector1=0          #I
    forward_frame_sector2=0          #I
    forward_frame_sector3=0          #I
    backward_frame_sectors0=0        #I

    backward_frame_sectors1=0        #I
    backward_frame_sectors2=0        #I
    backward_frame_sectors3=0        #I
    row_offsets0=0                   #H

    fmt="\
        II4sBBBB\
        IIBBBBI\
        HHIII\
        BBBBIII\
        IIII\
        IIII\
        IIIH\
         "
    b = struct.pack(fmt,\
                    mark0, mark1, magic, major, minior, debug, spare,\
                    sector_number, frame_number, hh, mm, ss, ff, header_words,\
                    width, height, image_words, audio_words, audio_freq,\
                    audio_channels, pad0, pad1, pad2, unused0, unused1, unused2,\
                    unused2, total_sectors, last_sector, forward_frame_sector0,\
                    forward_frame_sector1, forward_frame_sector2, forward_frame_sector3, backward_frame_sectors0,\
                    backward_frame_sectors1, backward_frame_sectors2, backward_frame_sectors3, row_offsets0\
                    )
    return (b)

def to_bcd(x):
    return ((x//10)*16 + (x%10))

def pad_sector(sector_offset, f):
    assert sector_offset < SECTOR_SIZE
    offset = sector_offset
    if sector_offset :
        for num in range(sector_offset, SECTOR_SIZE):
            f.write(struct.pack("B",0))
            offset=offset+1
    return (offset)

if __name__ == "__main__":
    main()
