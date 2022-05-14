#include <stdlib.h>
#include "main_config.h"

__attribute__((section(".mvs_config")))
const main_config g_main_config = {
    0xABCD,      //param1
    0x55AA,      //param2
    0x07,        //flag1
    0x99,        //value1
    "check sum", //a_name
};
