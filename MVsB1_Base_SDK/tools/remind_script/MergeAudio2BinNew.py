#!/usr/bin/python
# -*- coding=gb2312 -*-

import os, sys, struct, re
import time
import getopt

ver = 'v4.4'
readsize = 1024
totalFileLis = []
forCrcCheck = []
headerLen = 0x4000
entryOffset = 16
baseAddr = 0

UNKNOW_DECODER = 0
WAV_DECODER = 2
FLAC_DECODER= 3
AAC_DECODER = 4
AIF_DECODER = 5
AMR_DECODER = 6
MP3_DECODER = 129
WMA_DECODER = 130
SBC_DECODER = 131
MSBC_DECODER = 132

decode_type_dict = {
'.wma': WMA_DECODER,
'.wmv': WAV_DECODER,
'.asf': WAV_DECODER,

'.mp2': MP3_DECODER,
'.mp3': MP3_DECODER,

'.sbc': SBC_DECODER,

'.wav': WAV_DECODER,

'.flac': FLAC_DECODER,

'.aac': AAC_DECODER,
'.mp4': AAC_DECODER,
'.m4a': AAC_DECODER,
'.mov': AAC_DECODER,

'.aif': AIF_DECODER,

'.amr': AMR_DECODER,
}

re_files = "^[A-Za-z0-9_]{1,32}\.(mp3|wav|sbc|bin)$"
#re_files = "^[A-Za-z0-9_]{1,32}\.(wav|bin)$"

CrcCCITTTable = [
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
]

COPYRIGHT = (
"/**\n"
" *******************************************************************************\n"
" * @file    sound_remind.h\n"
" * @author  castle (Automatic generated)\n"
" * @version %s\n"
" * @date    %s\n"
" * @brief   \n"
" *******************************************************************************\n"
" * @attention\n"
" *\n"
" * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS\n"
" * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE\n"
" * TIME. AS A RESULT, MVSILICON SHALL NOT BE HELD LIABLE FOR ANY DIRECT,\n"
" * INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING\n"
" * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE\n"
" * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.\n"
" *\n"
" * <h2><center>&copy; COPYRIGHT 2018 MVSilicon </center></h2>\n"
" */\n"
"\n"
)

def get_audio_file_type(filename):
    name, ext = os.path.splitext(filename)
    if ext.lower() in decode_type_dict:
        return decode_type_dict[ext.lower()]
    else:
        return UNKNOW_DECODER

#convert filename to index number
#"0123.mp3" => int("0123")
def fileIdx(filename):
    return int(1)
    #return int(filename[:(filename.index('.')-len(filename))])

def fileIdx2(filename):
    name, ext = os.path.splitext(filename)
    if len(name)>=8:
        return name[0:8]
    else:
        return name+' '*(8-len(name))

def joinFile(fromdir, tofile, need_append):
    offset = 0
    output = open(tofile, 'wb')
    parts  = os.listdir(fromdir)
    partss = []

    #filter names with no ascii and not xxx.mp3
    for filename in parts:
        if re.compile(re_files).match(filename):
            partss.append(filename)

    for filename in partss:
        #print "reading %s, offset = %08X"%(filename, offset);
        filepath = os.path.join(fromdir, filename)
        if need_append:
            totalFileLis.append((fileIdx2(filename), offset+baseAddr+headerLen, os.path.getsize(filepath), 0, filename))

        offset += os.path.getsize(filepath)

        with open(filepath, 'rb') as f:
            output.write(f.read())

    output.close()

def do_clean_work():
    os.system('rm ./temp/*.bin')
    #os.system('del *.axf')
    os.system('rm *.bin')
    os.system('rm *.h')

def get_path():
    # determine if application is a script file or frozen exe
    if getattr(sys, 'frozen', False):
        application_path = os.path.dirname(sys.executable)
    elif __file__:
        application_path = os.path.dirname(__file__)

    return application_path

def CRC16(Buf, Offset, BufLen, CRC):
    for i in range(0, BufLen):
        CRC = ((CRC << 8) ^ CrcCCITTTable[((CRC >> 8) ^ Buf[i+Offset]) & 0x00FF]) & 0xffff
    return CRC

def is_number(s):
    try:
        int(s, 16)
        return True
    except ValueError:
        return False

def usage():
    print ("\nThis is the usage function")
    print ('Usage:')
    print ('-c, --clean : clean all temp files and result bin files')
    print ('-a, --addr []: specify the base address, eg: --addr 0xA0000')
    print ('-f, --filter []: filter the audio file, eg: --filter mp3')
    print ('-i, --inputdir []: location of input files dir, eg: --inputdir ..\\remind_sound')
    print ('-l, --hdrlen []: the data length in bytes for index header, eg: --hdrlen 0x1000')


if __name__=="__main__":
    #set current path
    print (time.asctime( time.localtime(time.time()) ))

    print ('MergeAudio2Bin Tools version %s for MVS SDK SoundRemind (Linux)'%(ver))
    print ('current path: ' + os.path.abspath(get_path()))
    os.chdir(get_path())

    #clean work:
    do_clean_work()
    baseAddr = 0x100000
    input_dir = 'ALI'

    try:
        options,args = getopt.getopt(sys.argv[1:], "hca:f:i:l:", ["help","clean","addr=","filter=","inputdir=","hdrlen="])
    except getopt.GetoptError as err:
        usage()
        sys.exit(0)

    for name,value in options:
        if name in ("-h","--help"):
            usage()
            sys.exit(0)
        if name in ("-c","--clean"):
            print("clean finished")
            sys.exit(0)
        if name in ("-a","--addr"):
            baseAddr = int(value, 16)
        if name in ("-f","--filter"):
            print("filter=%s"%(value))
            re_files = "^[A-Za-z0-9_]{1,32}\.(%s|bin)$"%(value)
        if name in ("-i","--inputdir"):
            print("inputdir=%s"%(value))
            input_dir = value
        if name in ("-l","--hdrlen"):
            headerLen = int(value, 16)

    if not os.path.isdir('temp'):
        os.mkdir('temp')

    if not os.path.isdir(input_dir):
        print('dir: \'%s\' not existed!!!'%(input_dir))
        sys.exit(0)

    print("base address = 0x%X"%(baseAddr))

    joinFile(input_dir, "./temp/BBBB.bin", 1)#package all mp3 files in BBBB.bin
    #for fileInfo in totalFileLis:
    #    print(fileInfo[0] + '&')

    with open("./temp/AAAA.bin", 'wb') as f:
        for i in range(0, headerLen):
            f.write(struct.pack("B", 0))

    with open('./temp/AAAA.bin', 'rb+') as f:
        f.write(struct.pack('4sLL', b'MVUB', 0, len(totalFileLis)))#sync flag and header length
        for fileInfo in totalFileLis:
            print ("%s: address = %08X, size = %08X, fullname = %s"%(fileInfo[0], fileInfo[1], fileInfo[2], fileInfo[4]))
            f.write(struct.pack('8sll', fileInfo[0].encode('utf-8'), fileInfo[1], fileInfo[2]))

    joinFile("temp", "all.bin", 0)
    print ("total file size=%d"%(os.path.getsize('all.bin')))
    with open("all.bin", 'rb') as f:
        data = f.read()
    for lstr in data:
        #print ord(lstr)
        forCrcCheck.append(lstr)
    crc16 = 0
    crc16 = CRC16(forCrcCheck, 0, 4, crc16)
    crc16 = CRC16(forCrcCheck, 8, len(forCrcCheck)-8, crc16)
    print ("CRC16 = 0x%04X"%crc16)

    #fill crc16
    forCrcCheck[4] = crc16 & 0xff
    forCrcCheck[5] = (crc16>>8) & 0xff
    with open("all.bin", 'wb+') as f:
        f.write(struct.pack('B' * len(forCrcCheck), *forCrcCheck))

    with open("sound_remind_item.h", 'w+') as f:
        #f.write(COPYRIGHT%(ver, time.asctime(time.localtime(time.time()))))
        f.write(COPYRIGHT%(ver, '20190101'))
        f.write("#ifndef __SOUND_REMIND__\n")
        f.write("#define __SOUND_REMIND__\n\n")
        f.write("#define SOUND_REMIND_TOTAL (%d)\n\n"%(len(totalFileLis)))
        for i, e in zip(range(len(totalFileLis)), totalFileLis):
            f.write("#define SOUND_REMIND_%s\t\"%s\"\t//%s\n"%(e[0].upper(), e[0], e[4]))
        f.write("\n#endif //__SOUND_REMIND__\n")
    #os.system('bin2hex.exe -b 0x0,all.bin -o sound_clips_data.axf')
    #os.system('copy sound_clips_data.axf ..\\output\\sound_clips_data.axf')#option
    print("finished...")
