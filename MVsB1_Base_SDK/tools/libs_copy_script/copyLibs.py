#!/usr/bin/python
# -*- coding=gb2312 -*-

import os, sys, struct, re
import time
import getopt
import shutil
import configparser

ver = 'v1.1.4'

onoff = 0

define_libs_list = []
default_dest_lib = ''
default_lib = ''
home_dir = ''
ini_config_file = 'script.ini'

class myconf(configparser.ConfigParser):
    def __init__(self,defaults=None):
        configparser.ConfigParser.__init__(self,defaults=None)
    def optionxform(self, optionstr):
        return optionstr

def get_path():
    # determine if application is a script file or frozen exe
    if getattr(sys, 'frozen', False):
        application_path = os.path.dirname(sys.executable)
    elif __file__:
        application_path, filename = os.path.split(os.path.realpath(sys.argv[0]))

    return application_path

def usage():
    print ("\nThis is the usage function")
    print ('Usage:')
    print ('-s, --script: ini file as copy script')
    
def force_to_copy_file(src, des):
    if not os.path.isfile(src):
        print("file %s not existed, exit."%(src))
        sys.exit(1)
    try:
        shutil.copyfile(src, des)
    except:
        print("shutil.copyfile failed...")
        sys.exit(1)

if __name__=="__main__":
    #set current path
    print (time.asctime( time.localtime(time.time()) ))

    print ('CopyLibs.py Tools version %s for MVS BT_audio SDK copy libs'%(ver))
    print ('current path: ' + os.path.abspath(get_path()))
    os.chdir(get_path())

    try:
        options,args = getopt.getopt(sys.argv[1:], "hs:", ["help", "script="])
    except getopt.GetoptError as err:
        usage()
        sys.exit(0)

    for name,value in options:
        if name in ("-h","--help"):
            usage()
            sys.exit(0)
        if name in ("-s","--script"):
            ini_config_file = value
    
    if not os.path.isfile(ini_config_file):
        print("file %s not existed"%(ini_config_file))
        sys.exit(1)
    
    config = myconf()
    config.read(ini_config_file)
    
    default_dest_lib = config['main_config'].get('default_dest_lib')
    default_lib = config['main_config'].get('default_lib')
    home_dir = config['main_config'].get('home_dir')

    #set home dir
    if home_dir:
        os.chdir(home_dir)
        print('home_dir = ' + os.getcwd())
        
    for key, var in config['macro_declare'].items():
        globals()[key] = int(var)
        define_libs_list.append(key)

    #searching *.h file to assign values to Macros
    for key, define_file in config['define_files'].items():
        # print(key, '=', define_file)
        with open(define_file, 'r', encoding='gbk') as f:
            # print('open file: %s'%(define_file))
            for oneline in f.readlines():
                for re_string in define_libs_list:
                    re_check = re.compile("^\s*#define\s*%s\s+(?P<logic>ENABLE|DISABLE|0|1)?"%(re_string)).match(oneline)
                    if re_check:
                        if re_check.groupdict()['logic']:
                            if re_check.groupdict()['logic'] == 'ENABLE' or re_check.groupdict()['logic'] == '1':
                                globals()[re_string] = 1
                            else:
                                globals()[re_string] = 0
                        else:
                            globals()[re_string] = 1
                        # print("1-->%s"%(oneline))
                        # print("%s=%d"%(re_string, globals()[re_string]))
                    re_check = re.compile("^\s*#define\s*%s$"%(re_string)).match(oneline)
                    if re_check:
                        globals()[re_string] = 1
                        # print("2-->%s"%(oneline))
                        # print("%s=%d"%(re_string, globals()[re_string]))

    #check the final valid express with TRUE
    the_express = ''
    for express, pathlib in config['macro_express'].items():
        if eval(express):
            the_express = express
            # print("see TRUE: %s"%(express))

    #do the final express action
    if the_express:
        print('DO REAL: ' + the_express)
        pathlib2 = config['macro_express'].get(the_express).replace(' ', '')
        if '>' in pathlib2:
            src, des = pathlib2.split('>', 1)
            dir, filename = os.path.split(src)
            print('copy %s'%(filename))
            force_to_copy_file(src, des)
        elif default_dest_lib:#default destination
            dir, filename = os.path.split(src)
            print('copy %s'%(filename))
            force_to_copy_file(pathlib2, default_dest_lib)
        else:
            print('no default_dest_lib defined.')
    else:
        print('doing default lib copy.')
        for key, copypath in config['default_copy'].items():
            copypath2 = copypath.replace(' ', '')
            src, des = copypath2.split('>', 1)
            dir, filename = os.path.split(src)
            print('copy %s'%(filename))
            force_to_copy_file(src, des)

    print('Copy Done.')
    sys.exit(0)


    

            

