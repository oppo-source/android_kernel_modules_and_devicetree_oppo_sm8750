/* http://srecord.sourceforge.net/ */
#include "tmf8806_image.h"

const PROGMEM unsigned char tmf8806_image[] =
{
0x6D, 0xC9, 0x42, 0x97, 0xDE, 0x38, 0xAC, 0xF2, 0x84, 0xED, 0xBF, 0x0D,
};
const unsigned long tmf8806_image_termination = 0x20000069;
const unsigned long tmf8806_image_start       = 0x20000000;
const unsigned long tmf8806_image_finish      = 0x2000000C;
const unsigned long tmf8806_image_length      = 0x0000000C;

#define TMF8806_IMAGE_TERMINATION 0x2000000B
#define TMF8806_IMAGE_START       0x20000000
#define TMF8806_IMAGE_FINISH      0x2000000C
#define TMF8806_IMAGE_LENGTH      0x0000000C
