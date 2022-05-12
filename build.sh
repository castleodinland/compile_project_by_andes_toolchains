#!/bin/bash
echo "begin to build auto_compile_project: $#"
workdir=$(cd $(dirname $0); pwd)
echo "$workdir"
if [ $# -ge 1 ]
then
	if [ "$1" = "build" ]
	then
		echo "build"
		python3 ./MVsB1_Base_SDK/tools/makefile_gen.py -i ./BT_Audio_APP/makefile.ini -d ./BT_Audio_APP
		cd ./BT_Audio_APP/make
		make all
		cd $workdir
	elif [ "$1" = "clean" ]
	then
		echo "clean"
		cd ./BT_Audio_APP/make
		make clean
		cd $workdir
	elif [ "$1" = "merge" ]
	then
		echo "merge audio files"
		python3 ./MVsB1_Base_SDK/tools/remind_script/MergeAudio2BinNew.py -a 0x0 -i ../../../BT_Audio_APP/remind_file
		cp -f ./MVsB1_Base_SDK/tools/remind_script/sound_remind_item.h ./BT_Audio_APP/bt_audio_app_src/inc/remind_sound_item.h
	elif [ "$1" = "copy" ]
	then
		echo "copy libs"
		python3 ./MVsB1_Base_SDK/tools/libs_copy_script/copyLibs.py -s ../../../BT_Audio_APP/script_copy.ini
	elif [ "$1" = "ldsag" ]
	then
		echo "ldsag generate"
		nds_ldsag -t ./MVsB1_Base_SDK/tools/nds32_template.txt ./BT_Audio_APP/nds32-ae210p.sag -o ./BT_Audio_APP/nds32-ae210p.ld
	else
		:
	fi
else
	echo "parameters error! [build][clean][merge][copy]"
fi


