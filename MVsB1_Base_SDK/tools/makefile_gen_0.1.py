#!/usr/bin/python
# -*- coding: UTF-8 -*- 

import os, sys, struct, re
import time
import getopt
import configparser

ver = 'v0.1'
date = '20210413'
ini_config_file = 'makefile.ini'
scan_dir = ''

re_cfile = "^*\.(c|s)$"

src_main_list = []
inc_full_path_list = []
compile_option_list = []
libs_list = []
libs_search_list = []
exclude_src_list = []

project_name = ''
objs_folder = ''

COPYRIGHT_SUBSRC_MK = (
"###############################################################################\n"
"# @file    subsrc.mk\n"
"# @author  castle (Automatic generated)\n"
"# @version %s\n"
"# @date    %s\n"
"# @brief   \n"
"# @attention\n"
"#\n"
"# THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS\n"
"# WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE\n"
"# TIME. AS A RESULT, MVSILICON SHALL NOT BE HELD LIABLE FOR ANY DIRECT,\n"
"# INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING\n"
"# FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE\n"
"# CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.\n"
"#\n"
"# <h2><center>&copy; COPYRIGHT 2021 MVSilicon </center></h2>\n"
"#/\n"
"###############################################################################\n"
"\n"
)

OBJ_LIBS_MK = (
"USER_OBJS :=\n"
"LIBS :="
)

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
        application_path = os.path.dirname(__file__)

    return application_path

def usage():
    print ("\nThis is the usage function")
    print ('Usage:')
    print ('-i, --inifile: ini file as config parameters')
    print ('-d, --scandir: project files(src, inc, etc.) searching dir')

#input need to be a full path
def fullpath_to_cygdrive(input):
    return os.path.join("/cygdrive/", input.replace(':', ''))

def is_exclude_source(src_list, filename):
    for name in exclude_src_list:
        if filename == name:
            return True
    return False

def main():
    global ini_config_file
    global project_name
    #set current path
    print (time.asctime( time.localtime(time.time()) ))

    print ('MVsilicon Makefile Generator version %s for MVS SDK.'%(ver))
    print ('current path: ' + os.path.abspath(get_path()))
    #os.chdir(get_path())

    try:
        options,args = getopt.getopt(sys.argv[1:], "hi:d:", ["help", "inifile=", "scandir="])
    except getopt.GetoptError as err:
        usage()
        sys.exit(0)

    for name,value in options:
        if name in ("-h","--help"):
            usage()
            sys.exit(0)
        if name in ("-i","--inifile"):
            ini_config_file = value
        if name in ("-d","--scandir"):
            scan_dir = value
    
    if scan_dir:
        scan_dir = os.path.abspath(scan_dir)
    print ('switch dir: ' + scan_dir)

    config = myconf()
    config.read(ini_config_file)

    config.get("brief_info", "project_name")

    project_name = config.get("brief_info", "project_name")
    make_folder = config.get("brief_info", "make_folder")
    objs_folder = config.get("brief_info", "objs_folder")

    if(not os.path.exists(scan_dir + '/' + make_folder)):
        os.mkdir(scan_dir + '/' +make_folder)
    if(not os.path.exists(scan_dir + '/' + make_folder + '/' + objs_folder)):
        os.mkdir(scan_dir + '/' + make_folder + '/' + objs_folder)
    
    for key, value in config['exclude_source'].items():
        exclude_src_list.append(value)

    for key, value in config['compile_option'].items():
        compile_option_list.append(value)
        # print(value)

    DEPS_MK = f"""
O_SRCS := 
C_SRCS := 
S_SRCS := 
S_UPPER_SRCS := 
OBJ_SRCS := 
ASM_SRCS := 
SAG_SRCS += \\

OBJCOPY_OUTPUTS := 
OBJS := 
C_DEPS := 
GCOV_OUT := 
SYMBOL_OUTPUTS := 
READELF_OUTPUTS := 
GPROF_OUT := 
SIZE_OUTPUTS := 
EXECUTABLES := 
S_UPPER_DEPS := 

SUBDIRS := \\
{objs_folder} \\

"""

    fomk = open(f"{scan_dir}/{make_folder}/subsrc.mk", "w+")
    fomk.write(COPYRIGHT_SUBSRC_MK%(ver, date))
    fomk.write(DEPS_MK)
    fomk.write(OBJ_LIBS_MK)

    for key, value in config['link_libs'].items():
        fomk.write(' -l'+value)
    fomk.write('\n')

    #collect all source file and fullpath name
    source_folder_list = []
    for key, value in config['source_folder'].items():
        source_folder_list.append(value)
    for search_folder in source_folder_list:
        parts = os.listdir(f"{scan_dir}/{search_folder}")
        for filename in parts:
            if is_exclude_source(exclude_src_list, filename):
                continue
            if os.path.splitext(filename)[-1][1:] == "c" or os.path.splitext(filename)[-1][1:] == "S":
                fullpath = os.path.join(os.path.abspath(f"{scan_dir}/{search_folder}"), filename)
                fullpath = fullpath.replace('\\', '/')
                #c o d fullpath fullpath-cygdrive
                o_filename = filename.replace(os.path.splitext(filename)[-1][1:], 'o')
                d_filename = filename.replace(os.path.splitext(filename)[-1][1:], 'd')
                cyg = fullpath_to_cygdrive(fullpath)
                item = (filename, o_filename, d_filename, fullpath, cyg)
                src_main_list.append(item)
    
    #collect all include path
    source_folder_list = []
    for key, value in config['header_folder'].items():
        source_folder_list.append(value)
    
    for search_folder in source_folder_list:
        fullpath = os.path.abspath(f"{scan_dir}/{search_folder}")
        fullpath = fullpath.replace('\\', '/')
        #fullpath fullpath-cygdrive
        cyg = fullpath_to_cygdrive(fullpath)
        item = (fullpath, cyg)
        inc_full_path_list.append(item)

    #C_SRCS += \
    fomk.write('\nC_SRCS += \\' + '\n')
    for item in src_main_list:
        fomk.write(item[3] + ' \\' + '\n')

    #OBJS += \
    fomk.write('\nOBJS += \\' + '\n')
    for item in src_main_list:
        fomk.write('./' + objs_folder + '/' + item[1] + ' \\' + '\n')

    #C_DEPS += \
    fomk.write('\nC_DEPS += \\' + '\n')
    for item in src_main_list:
        fomk.write('./' + objs_folder + '/' + item[2] + ' \\' + '\n')

    defined_symbols_long_str = ''
    for key, value in config['defined_symbols'].items():
        defined_symbols_long_str += '-D' + value + ' '

    #make rules
    for item in src_main_list:
        fomk.write('\n' + objs_folder + '/' + item[1] + ': ' + item[3] + '\n')
        fomk.write('\t' + '@echo \'Building file: $<\'' + '\n')
        fomk.write('\t' + '@echo \'Invoking: Andes C Compiler\'' + '\n')
        fomk.write('\t' + '$(CROSS_COMPILE)gcc ' + defined_symbols_long_str)

        for incs in inc_full_path_list:
            fomk.write('-I\"' + incs[1] + '\" ')
        for compile_opt in compile_option_list:
            fomk.write(compile_opt + ' ')
        fomk.write('\n')
        fomk.write('\t' + '@echo \'Finished building: $<\'' + '\n')
        fomk.write('\t' + '@echo \' \'' + '\n')
        
    fomk.close()

####################################################################################
    
    libs_search_long_str = ''
    for key, value in config['link_libs_search'].items():
        fullpath = os.path.abspath(f"{scan_dir}/{value}")
        fullpath = fullpath.replace('\\', '/')
        #fullpath fullpath-cygdrive
        cyg = fullpath_to_cygdrive(fullpath)
        libs_search_long_str += '-L\"%s\" '%(cyg)

    link_option_long_str = ''
    for key, value in config['link_option'].items():
        link_option_long_str += value + ' '


    MAIN_MAKEFILE = f"""
###############################################################################
# @file    makefile
# @author  castle (Automatic generated)
# @version {ver}
# @date    {date}
# @brief   
# @attention
#
# THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
# WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
# TIME. AS A RESULT, MVSILICON SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
# INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
# FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
# CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
#
# <h2><center>&copy; COPYRIGHT 2021 MVSilicon </center></h2>

###############################################################################
ifndef ANDESIGHT_ROOT
ANDESIGHT_ROOT=/cygdrive/C/Andestech/AndeSight300STD
endif

ifndef CROSS_COMPILE
CROSS_COMPILE=nds32le-elf-
endif

ifndef SECONDARY_OUTPUT_PATH
SECONDARY_OUTPUT_PATH=output
endif

$(shell mkdir -p $(SECONDARY_OUTPUT_PATH))

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
#-include sources.mk
-include subsrc.mk
#-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 
BUILD_ARTIFACT_FILE_BASE_NAME = {project_name}

LINKER_OUTPUTS = {project_name}.adx

GCOV_OUT += \\
$(foreach subdir,$(SUBDIRS),$(wildcard $(subdir)/*.gcda) $(wildcard $(subdir)/*.gcno)) \\

GPROF_OUT += \\
../gmon.sum ../gmon.out \\

SYMBOL_OUTPUTS += \\
$(SECONDARY_OUTPUT_PATH)/symbol.txt \\

READELF_OUTPUTS += \\
$(SECONDARY_OUTPUT_PATH)/readelf.txt \\

OBJDUMP_OUTPUTS += \\
$(SECONDARY_OUTPUT_PATH)/objdump.txt \\

OBJCOPY_OUTPUTS += \\
$(SECONDARY_OUTPUT_PATH)/$(BUILD_ARTIFACT_FILE_BASE_NAME).bin \\

SIZE_OUTPUTS += \\
$(SECONDARY_OUTPUT_PATH)/.PHONY.size \\


# All Target
all: pre-build 
	$(MAKE) main-build

# Main-build Target
main-build: {project_name}.adx secondary-outputs

# Tool invocations
{project_name}.adx: $(OBJS) $(USER_OBJS)
\t@echo 'Building target: $@'
\t@echo 'Invoking: Andes C Linker'
\t$(CROSS_COMPILE)gcc {libs_search_long_str} {link_option_long_str} -o "{project_name}.adx" $(OBJS) $(USER_OBJS) $(LIBS)
\t@echo 'Finished building target: $@'
\t@echo ' '

$(foreach subdir,$(SUBDIRS),$(wildcard $(subdir)/*.gcda) $(wildcard $(subdir)/*.gcno)): {project_name}.adx $(OBJS) $(USER_OBJS)
../gmon.sum ../gmon.out: {project_name}.adx $(OBJS) $(USER_OBJS)

$(SECONDARY_OUTPUT_PATH)/symbol.txt: $(LINKER_OUTPUTS)
\t@echo 'Invoking: NM (symbol listing)'
\t$(CROSS_COMPILE)nm -n -l -C "{project_name}.adx" > $(SECONDARY_OUTPUT_PATH)/symbol.txt
\t@echo 'Finished building: $@'
\t@echo ' '

$(SECONDARY_OUTPUT_PATH)/readelf.txt: $(LINKER_OUTPUTS)
\t@echo 'Invoking: Readelf (ELF info listing)'
\t$(CROSS_COMPILE)readelf -a "{project_name}.adx" > $(SECONDARY_OUTPUT_PATH)/readelf.txt
\t@echo 'Finished building: $@'
\t@echo ' '

$(SECONDARY_OUTPUT_PATH)/$(BUILD_ARTIFACT_FILE_BASE_NAME).bin: $(LINKER_OUTPUTS)
\t@echo 'Invoking: Objcopy (object content copy)'
\t$(CROSS_COMPILE)objcopy -S -O binary "{project_name}.adx" $(SECONDARY_OUTPUT_PATH)/$(BUILD_ARTIFACT_FILE_BASE_NAME).bin
\t@echo 'Finished building: $@'
\t@echo ' '

$(SECONDARY_OUTPUT_PATH)/.PHONY.size: $(LINKER_OUTPUTS)
\t@echo 'Invoking: Size (section size listing)'
\t$(CROSS_COMPILE)size  "{project_name}.adx" | tee $(SECONDARY_OUTPUT_PATH)/.PHONY.size
\t@echo 'Finished building: $@'
\t@echo ' '

$(SECONDARY_OUTPUT_PATH)/objdump.txt: $(LINKER_OUTPUTS)
\t@echo 'Invoking: Objdump (disassembly)'
\t$(CROSS_COMPILE)objdump -x -d -C "{project_name}.adx" > $(SECONDARY_OUTPUT_PATH)/objdump.txt
\t@echo 'Finished building: $@'
\t@echo ' '

# Other Targets
clean:
\t-$(RM) $(OBJCOPY_OUTPUTS)$(OBJS)$(C_DEPS)$(GCOV_OUT)$(SYMBOL_OUTPUTS)$(OBJDUMP_OUTPUTS)$(READELF_OUTPUTS)$(GPROF_OUT)$(SIZE_OUTPUTS)$(EXECUTABLES)$(S_UPPER_DEPS) {project_name}.adx
\t-@echo ' '

pre-build:
	-..\script.bat
	-@echo ' '

secondary-outputs: $(SYMBOL_OUTPUTS) $(READELF_OUTPUTS) $(OBJDUMP_OUTPUTS) $(OBJCOPY_OUTPUTS) $(SIZE_OUTPUTS)

.PHONY: all clean dependents
.SECONDARY: main-build pre-build
.NOTPARALLEL: pre-build

-include ../makefile.targets

"""

    fomk = open(f"{scan_dir}/{make_folder}/makefile", "w+")
    fomk.write(MAIN_MAKEFILE)
    fomk.close()
    
if __name__=="__main__":
    main()
