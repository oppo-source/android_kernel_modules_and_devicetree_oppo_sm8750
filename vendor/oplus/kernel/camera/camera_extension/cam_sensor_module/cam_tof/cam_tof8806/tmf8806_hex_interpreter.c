/*
 *****************************************************************************
 * Copyright by ams OSRAM AG                                                 *
 * All rights are reserved.                                                  *
 *                                                                           *
 * IMPORTANT - PLEASE READ CAREFULLY BEFORE COPYING, INSTALLING OR USING     *
 * THE SOFTWARE.                                                             *
 *                                                                           *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS       *
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT         *
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS         *
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT  *
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,     *
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT          *
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,     *
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY     *
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT       *
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE     *
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.      *
 *****************************************************************************
 */

/** @file This is the hex interpreter for FW Download
 */

#include "tmf8806_hex_interpreter.h"
#include <linux/printk.h>
#include "tmf8806.h"

/*
 *****************************************************************************
 * TYPES
 *****************************************************************************
 */

/* hold the one currently interpreted intel hex record */
typedef struct _intelHexRecord
{
	unsigned int ulba; /* ULBA = upper linear base address = higher 16 bits of the 32-bit address */
	unsigned int address; /* lower 16-bit of the 32-bit address - old intel formats used just 16-bit addresses */
	unsigned int length; /* number of valid elements in data array */
	uint8_t data[ INTEL_HEX_MAX_RECORD_DATA_SIZE ];
} intelRecord;

/*
 *****************************************************************************
 * VARIABLES
 *****************************************************************************
 */
static intelRecord intelHexRecord; /* converted ascii intel hex input into an ready to program data record */
static unsigned int lastAddress;
unsigned char patchImage[MAX_PATCH_IMAGE_SIZE];
int imageSize;

/*
 *****************************************************************************
 * FUNCTIONS
 *****************************************************************************
 */
/* intel hex record functions */
static uint8_t asciiToBinaryNibble( uint8_t a, uint8_t * error );
static uint8_t asciiToBinaryByte( const uint8_t * * linePtr, uint8_t * error );
static uint8_t fillImageWithRecordData( tmf8806_chip *chip );
/*****************************************************************************/

void intelHexInterpreterInitialise8806( void )
{
	int i;
	intelHexRecord.address = 0;
	intelHexRecord.ulba = 0;
	intelHexRecord.length = 0; /* no data in record valid */
	for ( i = 0; i < INTEL_HEX_MAX_RECORD_DATA_SIZE; i++ )
	{
		intelHexRecord.data[ i ] = 0;
	}
	lastAddress = 0xFFFFFFFF;
	imageSize = 0;
}

uint8_t intelHexHandleRecord8806( tmf8806_chip *chip, uint8_t lineLength, const uint8_t * line)
{
	uint8_t result = INTEL_HEX_ERR_TOO_SHORT;
	if ( lineLength && line[ 0 ] == ':' ) /* intel hex records must start with a colon */
	{
		/* looks promising -> we found the starting uint8_tacter of an intel hex record. */
		uint8_t crc;
		uint8_t type;
		uint8_t data;
		int i;

		/* intel hex records we interpret as follow:
		   :llaaaattdddd...dddcc
		   l=length a=address t=type d=data c=checksum
		   So an intel hex record has as 11 uint8_tacters that
		   are not data.
		 */

		if ( lineLength < INTEL_HEX_MIN_LAST_ADDRESS )
		{
			return INTEL_HEX_ERR_TOO_SHORT;
		}
		else
		{
			result = INTEL_HEX_CONTINUE;
			lineLength -= INTEL_HEX_MIN_LAST_ADDRESS; /* substract the minimum address from
			the last written address -> out comes the lineLength (of the real data) */

			/* 1. the first uint8_tacter (':') has to be eaten */
			line++;

			/* 2. read length - 2 ascii = 8 bit value */
			intelHexRecord.length = asciiToBinaryByte( &line, &result );
			crc = intelHexRecord.length; /* start calculating crc */

			/* 3. read address - 4 ascii = 16 bit value */
			intelHexRecord.address = asciiToBinaryByte( &line, &result );
			crc += intelHexRecord.address;
			intelHexRecord.address <<= 8; /* move up by 1 byte */
			data = asciiToBinaryByte( &line, &result );
			crc += data;
			intelHexRecord.address += data;

			/* 4. read type - 2 ascii = 8 bit value */
			type = asciiToBinaryByte( &line, &result );
			crc += type;

			if ( ( intelHexRecord.length * 2 ) > lineLength )
			{
				return INTEL_HEX_ERR_TOO_SHORT;
			}
			else /* record still valid */
			{
				for ( i = 0; i < intelHexRecord.length; ++i )
				{ /* fill data into record */
					data = asciiToBinaryByte( &line, &result );
					crc += data;
					intelHexRecord.data[ i ] = data;
				}

				/* read crc and compare */
				data = asciiToBinaryByte( &line, &result );
				crc = ( -crc ) & 0xff;
				if ( result != INTEL_HEX_CONTINUE )
				{
					return result;
				}
				else if ( crc != data )
				{
					return INTEL_HEX_ERR_CRC_ERR;
				}
				else /* record still valid */
				{
					/* depending on the type we interpret the data differently and must adjust the length */
					if ( type == INTEL_HEX_TYPE_DATA )
					{
						result = INTEL_HEX_CONTINUE; /* not an EOF record - conversion errors directly lead to an abort */
					}
					else if ( type == INTEL_HEX_TYPE_EOF )
					{
						intelHexRecord.length = 0;
						intelHexRecord.ulba = 0xFFFFFFFFUL; /* impossible ULBA (only upper 16-bits are allowed to be non-zero) */
						result = INTEL_HEX_EOF;
					}
					else if ( type == INTEL_HEX_TYPE_EXT_LIN_ADDR )
					{
						intelHexRecord.ulba = intelHexRecord.data[ 0 ];
						intelHexRecord.ulba <<= 8;
						intelHexRecord.ulba |= intelHexRecord.data[ 1 ];
						intelHexRecord.ulba <<= 16;
						intelHexRecord.length = 0; /* no other valid data */
						result = INTEL_HEX_CONTINUE; /* not an EOF, - conversion errors directly lead to an abort */;
					}
					else if ( type == INTEL_HEX_TYPE_START_LIN_ADDR )
					{
						intelHexRecord.length = 0;
						result = INTEL_HEX_CONTINUE; /* not an EOF, - conversion errors directly lead to an abort */;
					}
					else
					{
						return INTEL_HEX_ERR_UNKNOWN_TYPE;
					}
				}
			}
		}

		if   ( result == INTEL_HEX_CONTINUE ) /* if it is an eof record we do not want to fill Image Data */
		{
			result = fillImageWithRecordData( chip );
		}
	}

	if ( result == INTEL_HEX_EOF )
	{
		result = 0; //result is ok
	}

	return result; /* valid data received within time */
}

/* ---------------  intel hex record handling ------------------ */

/* convert an ascii nibble to its binary value exit loader in case
   that the ascii uint8_tacter does not represent a number */
static uint8_t asciiToBinaryNibble ( uint8_t a, uint8_t * error )
{
	uint8_t b = a - '0';
	if ( b > 9 )
	{
		b = a - 'A';
		if ( b > 5 )  /* 'A'..'F': note that we still have to add 10 */
		{
			if ( error )
			{
				*error = INTEL_HEX_ERR_NOT_A_NUMBER;
			}
			b = 0;
		}
		b += 10; /* 'A'..'F' means 10, 11, 12, 13,.. not 1, 2, ... 6 */
	}
	return b;
}

/* convert 2 ascii uint8_tacters into a single byte - if possible.
   flag an error in variable intelHexError in case
   that the ascii uint8_tacter does not represent a number */
static uint8_t asciiToBinaryByte ( const uint8_t * * linePtr, uint8_t * error )
{
	const uint8_t * line = *linePtr;
	(*linePtr) += 2;
	return ( asciiToBinaryNibble( *line, error ) << 4 ) | asciiToBinaryNibble( *(line+1), error );
}
/**
 * fillImageWithRecordData
 *
 * @tmf8806_chip: tmf8806_chip pointer
 * Returns 0 no error, otherwise error
 */
static uint8_t fillImageWithRecordData( tmf8806_chip *chip )
{
	uint8_t result = 0;
	unsigned int address = intelHexRecord.ulba + intelHexRecord.address;
	int i;

	if ( address != lastAddress ) /* need to set new address */
	{
		if ((lastAddress != 0xFFFFFFFF) && (intelHexRecord.length)) /* support only for hex file without gaps */
		{
			result = READ_ERROR;
		}
		lastAddress = address;
	}

	if ( intelHexRecord.length )
	{ /* have data to write */

		/* continues write now - cause either we set up the new address or it was continues;*/
		if ( chip->tof_core.logLevel & TMF8806_LOG_LEVEL_VERBOSE )
		{
			PRINT_STR("Addr:");
			PRINT_UINT(address);
			PRINT_LN();
			PRINT_STR( "Record Length:" );
			PRINT_INT(intelHexRecord.length);
			PRINT_LN();
			PRINT_STR( "Record:" );
		}
		for ( i = 0; i < intelHexRecord.length; i++)
		{
			patchImage[imageSize] = intelHexRecord.data[i];
			imageSize++;
			if ( chip->tof_core.logLevel & TMF8806_LOG_LEVEL_VERBOSE )
			{
				PRINT_INT( intelHexRecord.data[i] );
				PRINT_CHAR( SEPARATOR );
			}
		}
		if ( chip->tof_core.logLevel & TMF8806_LOG_LEVEL_VERBOSE )
		{
			PRINT_LN( );
		}
		/* increment last address */
		lastAddress += intelHexRecord.length;
	}
	return result;
}
