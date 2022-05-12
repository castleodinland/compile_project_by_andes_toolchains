#!/usr/bin/python
# -*- coding: UTF-8 -*- 

import os, sys, struct, re
import time
import getopt
import configparser

object_folder = 'objs'
output_folder = 'output'

source_and_section = {'main_config': '.mvs_config'}

re_good_line = r'\s*\[\d{1,8}\]\s*(%s)\s*PROGBITS\s*[0-9a-fA-F]{8}\s*([0-9a-fA-F]{6})\s*([0-9a-fA-F]{6})([\w\.]*)'

def get_path_ex():
    # determine if application is a script file or frozen exe
    if getattr(sys, 'frozen', False):
        application_path = os.path.dirname(sys.executable)
    elif __file__:
        application_path = os.path.dirname(__file__)

if __name__=="__main__":
    main_local_path = get_path_ex()
    # os.system('nds32le-elf-readelf.exe -S BT_Audio_APP\make\objs\main_config.o > BT_Audio_APP\make\objs\main_config.txt')

    for key, value in source_and_section.items():
        # print('process: %s'%(key))
        if not os.path.isfile('%s\%s.o'%(object_folder, key)):
            continue

        content = os.popen('nds32le-elf-readelf.exe -S objs\%s.o'%(key)).read()
        # print(content)
        for a_line in content.split('\n'):
            # print('>>' + a_line)
            pattern1 = re.compile(re_good_line%(value))
            match_obj = pattern1.match(a_line)
            if match_obj:
                print(a_line)
                data_start = int(match_obj.group(2), 16)
                data_size = int(match_obj.group(3), 16)
                # print("find %s: %d, %d"%(match_obj.group(1), data_start, data_size))
                in_file = open('%s\%s.o'%(object_folder, key), 'rb')
                out_file = open('%s\%s.bin'%(output_folder, key), 'wb')

                in_file.seek(data_start, 0)

                for i in range(data_size):
                    data = in_file.read(1)
                    out_file.write(data)

                in_file.close()
                out_file.close()

                break
