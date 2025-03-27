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

/** @file This is the shim for raspberry pi
 * Defines, macro and functions to match the target platform.
 */

#include <linux/delay.h>
#include <linux/types.h>
#include <linux/gpio.h>
#include <linux/timekeeping.h>
#include <linux/math64.h>

#include "tmf8806_driver.h"
#include "tmf8806_shim.h"

int readPin( struct gpio_desc *gpiod )
{
	return(gpiod_get_value(gpiod) ? 1 : 0);
}

int writePin( struct gpio_desc *gpiod, uint8_t value )
{
	return gpiod_direction_output(gpiod, value);
}

void delayInMicroseconds ( uint32_t wait )
{
	usleep_range(wait, wait + wait/10);
}

uint32_t getSysTick ( void ) //Note: is only for 70 minutes
{
	u64 ktime_us = div_u64(ktime_get_ns(), NSEC_PER_USEC);
	return (uint32_t)ktime_us;
}

uint8_t readProgramMemoryByte ( uint64_t address )
{
	uint8_t byte = *(uint64_t *)address;
	return byte;
}

int8_t tmf8806ReadQuadHistogram ( void * dptr, uint8_t slaveAddr, uint8_t * buffer )
{
	return i2cRxReg( dptr, slaveAddr, TMF8806_COM_RESULT_NUMBER, 128, buffer );
}

int8_t i2cTxReg ( void * dptr, uint8_t slaveAddr, uint8_t regAddr, uint16_t toTx, const uint8_t * txData )
{
	tmf8806_chip * driver = (tmf8806_chip *)dptr;
	int error;
	char debug[120]; //max for debug
	int idx;
	u32 strsize = 0;

	error = i2c_write(driver->client, regAddr, txData, toTx);
	if ( error)
	{
		return I2C_ERROR;
	}

	if ( driver->tof_core.logLevel & TMF8806_LOG_LEVEL_I2C )
	{
		strsize = scnprintf(debug, sizeof(debug), "i2cTx: Reg:%02x Len:%hx Dat: ", regAddr, toTx);
		for(idx = 0; idx < toTx; idx++) {
			strsize += scnprintf(debug + strsize, sizeof(debug) - strsize, "%02x ", txData[idx]);
		}
		dev_info(&driver->client->dev, "%s", debug);
	}

	return I2C_SUCCESS;
}

int8_t i2cRxReg ( void * dptr, uint8_t slaveAddr, uint8_t regAddr, uint16_t toRx, uint8_t * rxData )
{
	tmf8806_chip * driver = (tmf8806_chip *)dptr;
	int error;
	char debug[120]; //max for debug
	int idx;
	u32 strsize = 0;

	error = i2c_read(driver->client, regAddr, rxData, toRx);

	if ( error)
	{
		return I2C_ERROR;
	}

	if ( driver->tof_core.logLevel & TMF8806_LOG_LEVEL_I2C )
	{
		strsize = scnprintf(debug, sizeof(debug), "i2cRx: Reg:%02x Len:%hx Dat:", regAddr, toRx);
		for(idx = 0; idx < toRx; idx++) {
			strsize += scnprintf(debug + strsize, sizeof(debug) - strsize, "%02x ", rxData[idx]);
		}
		dev_info(&driver->client->dev, " %s", debug);
	}

	return I2C_SUCCESS;
}

void tmf8806ScaleAndPrintHistogram ( void * dptr, uint8_t histType, uint8_t id, uint8_t * data, uint8_t scale )
{
	tmf8806_chip * driver = (tmf8806_chip *)dptr;
	int payload_in_buffer = (driver->tof_output_frame.frame.payload_msb << 8) + driver->tof_output_frame.frame.payload_lsb;
	int payload_size = payload_in_buffer + (TMF8806_HISTOGRAM_BINS*2);
	driver->tof_output_frame.frame.frameId = histType;
	driver->tof_output_frame.frame.payload_lsb = payload_size & 0xFF;
	driver->tof_output_frame.frame.payload_msb = (payload_size >> 8) & 0xFF;


	memcpy(&driver->tof_output_frame.frame.payload[payload_in_buffer], data, (TMF8806_HISTOGRAM_BINS*2) ); // from core driver to linux driver

	if ( driver->tof_core.logLevel >= TMF8806_LOG_LEVEL_VERBOSE )
	{
		switch ( histType )
		{
			case TMF8806_DIAG_HIST_ALG_PILEUP:	  // Pileup histogram
			{ // pileup == 4x 1histogram, shift bin-content by 1
				PRINT_STR( "#TGPUC" );
			}
			break;
			case TMF8806_DIAG_HIST_ALG_PU_TDC_SUM:	  // SUM histogram
			{ // sum = 1x histogram, shift bin-content by 2
				PRINT_STR( "#SUM" );
			}
			break;
			case TMF8806_DIAG_HIST_ELECTRICAL_CAL:
			{ // 5 histograms
				PRINT_STR( "#CI" );
			}
			break;
			case TMF8806_DIAG_HIST_PROXIMITY:
			{ // 5 histograms
				PRINT_STR( "#PT" );
			}
			break;
			case TMF8806_DIAG_HIST_DISTANCE:
			{ // 5 histograms
				PRINT_STR( "#TG" );
			}
			break;
			default:
			{
				PRINT_STR( "#UNKNOWN" );
			}
			break;
		}
		PRINT_INT( (id&0x1F)/2 ); // TDC+Channel number
		for ( uint8_t i = 0; i < TMF8806_HISTOGRAM_BINS; i++, data += 2 )
		{
			uint32_t value = tmf8806GetUint16(data);	  // little endian
			value = value << scale;							 // print scaled histograms -> can become 32-bit
			PRINT_CHAR( SEPARATOR );
			PRINT_INT( value );
		}
		PRINT_LN( );
	}
}

int enablePinHigh ( void * dptr )
{
	tmf8806_chip * driver = (tmf8806_chip *)dptr;
	return writePin( driver->pdata->gpiod_enable, 1);
}

int enablePinLow ( void * dptr )
{
	tmf8806_chip * driver = (tmf8806_chip *)dptr;
	return writePin( driver->pdata->gpiod_enable, 0);
}

int8_t tmf8806IsApp0Ready ( tmf8806Driver * driver , uint8_t waitInMs )
{
	uint16_t timeout100Us = waitInMs * 10;
	uint32_t t = getSysTick( );
	do
	{
		if (tmf8806ReadDeviceInfo( driver ) == APP_SUCCESS_OK)
		{
			if ( driver->logLevel >= TMF8806_LOG_LEVEL_VERBOSE )
			{
			  PRINT_STR( "App0 ready" );
			  PRINT_LN( );
			}
			return 1;  // done
		}
		else if ( timeout100Us ) // only wait until it is the last time we go through the loop, that would be a waste of time to wait again
		{
			delayInMicroseconds( 100 );
		}
	} while ( timeout100Us-- );
	if ( driver->logLevel >= TMF8806_LOG_LEVEL_ERROR )
	{
		t = getSysTick( ) - t;   // duration
		PRINT_STR( "#Err" );
		PRINT_CHAR( SEPARATOR );
		PRINT_STR( "App0 not ready waited" );
		PRINT_CHAR( SEPARATOR );
		PRINT_INT( t );
		PRINT_LN( );
	}
	return 0;  // cpu did not get ready
}

int tmf8806_app_process_irq( void * dptr )
{
	uint8_t intStatus = 0;
	int8_t res = APP_SUCCESS_OK;
	int inthandled = 0;
	int first_obj_dis = 0;
	unsigned int xtalk_peak = 0;
	tmf8806_chip *driver = (tmf8806_chip *)dptr ;

	tmf8806DistanceResultFrame resultFrame;

	intStatus = tmf8806GetAndClrInterrupts(&driver->tof_core, TMF8806_INTERRUPT_RESULT | TMF8806_INTERRUPT_DIAGNOSTIC);
	if ( driver->tof_core.logLevel >= TMF8806_LOG_LEVEL_VERBOSE )
	{
		PRINT_INT(intStatus);
	}
	if ( intStatus & TMF8806_INTERRUPT_RESULT )
	{ // check if a result is available
		res = tmf8806ReadResult( &driver->tof_core, &resultFrame );
		if (res == APP_SUCCESS_OK)
		{
			if(driver->tof_core.clkCorrectionEnable)
			{
				resultFrame.distPeak = tmf8806CorrectDistance( &driver->tof_core, resultFrame.distPeak );
				tmf8806SetUint16(resultFrame.distPeak, &driver->tof_core.dataBuffer[RESULT_REG(RESULT_NUMBER)+2]);
				//calculate xtalk average
				xtalk_peak = resultFrame.xtalk;
				if (driver->xtalk_count < TOF_XTALK_AVERAGE_COUNT) {
					driver->xtalk_count++;
				}
				driver->xtalk_peak = ((driver->xtalk_peak * (driver->xtalk_count - 1)) + xtalk_peak) / driver->xtalk_count;
				if (driver->driver_debug) {
					CAM_EXT_INFO(CAM_EXT_TOF, "xtalk peak: current %u average %u", xtalk_peak, driver->xtalk_peak);
				}

				CAM_EXT_INFO(CAM_EXT_TOF, "distPeak = %u,resultNum =%u,reliability=%u,referenceHits=%u,objectHits=%u,xtalk=%u",
															resultFrame.distPeak,
															resultFrame.resultNum,
															resultFrame.reliability,
															resultFrame.referenceHits,
															resultFrame.objectHits,
															resultFrame.xtalk
															);
			}
			first_obj_dis |= resultFrame.distPeak;
			first_obj_dis |= (resultFrame.reliability)<<16;
			first_obj_dis |= (resultFrame.resultNum)<<24;
			input_event(driver->obj_input_dev,EV_ABS, ABS_DISTANCE,first_obj_dis);
			input_sync(driver->obj_input_dev);

			driver->tof_output_frame.frame.frameId = TMF8806_COM_RESULT__measurement_result;
			driver->tof_output_frame.frame.payload_lsb = TMF8806_COM_RESULT__measurement_result_size & 0xFF;
			driver->tof_output_frame.frame.payload_msb = (TMF8806_COM_RESULT__measurement_result_size >> 8) & 0xFF;
			memcpy(driver->tof_output_frame.frame.payload, driver->tof_core.dataBuffer, TMF8806_COM_RESULT__measurement_result_size ); // from core driver to linux driver
			tof8806_queue_frame(driver);
		}
		inthandled = 1;
	}

	if (intStatus & TMF8806_INTERRUPT_DIAGNOSTIC)// check if a histogram is available
	{
		res = tmf8806ReadHistograms( &driver->tof_core );
		tof8806_queue_frame(driver);
		inthandled = 1;
	}


	return inthandled;
}

int tmf8806_oscillator_trim( tmf8806Driver * driver, int *trim_value, char write )
{
	uint8_t fuse3 = 0;
	uint8_t fuse6 = 0;
	uint8_t sopw = 0x29;
	uint8_t regVal = 0;
	int trim_val = 0;

	if (tmf8806StopMeasurement( driver) != APP_SUCCESS_OK) //Stop Running Measurement
	{
		return I2C_ERROR;
	}

	//Write sopw and go to Standby
	if (i2cTxReg( driver, 0, APP0_CMD_DATA_9, 1, &sopw ) != APP_SUCCESS_OK)
	{
		return I2C_ERROR;
	}

	tmf8806Standby(driver);
	delayInMicroseconds( ENABLE_TIME_MS * 1000  );
	if (i2cRxReg(driver, 0, 0xE0, 1, &regVal) != APP_SUCCESS_OK || (regVal != 0x0))
	{
		PRINT_STR( "not Standby" );
		return I2C_ERROR;
	}

	// adjust or read osc trim value
	if (i2cRxReg(driver, 0, 0x03, 1, &fuse3) != APP_SUCCESS_OK)
	{
		return I2C_ERROR;
	}
	if (i2cRxReg(driver, 0, 0x06, 1, &fuse6) != APP_SUCCESS_OK)
	{
		return I2C_ERROR;
	}

	if (write)
	{
		trim_val = *trim_value;
		trim_val &= 0x1FF;
		fuse6 &= ~(0x01 << 6);
		fuse6 |= ((trim_val & 0x01) << 6);
		fuse3  = ((trim_val >> 1) & 0xFF);
		if (i2cTxReg( driver, 0, 0x03, 1, &fuse3 ) != APP_SUCCESS_OK)
		{
			return I2C_ERROR;
		}
		if (i2cTxReg( driver, 0, 0x06, 1, &fuse6 ) != APP_SUCCESS_OK)
		{
			return I2C_ERROR;
		}
	}
	else
	{
		*trim_value = (((int)fuse3) << 1) | ((fuse6 & (0x01 << 6)) >> 6);
	}

	//wake up device
	tmf8806Wakeup(driver);
	delayInMicroseconds( ENABLE_TIME_MS * 1000  );
	if (i2cRxReg(driver, 0, 0xE0, 1, &regVal) != APP_SUCCESS_OK || ((regVal & 0x01) != 0x1))
	{
		PRINT_STR( "not Wakeup" );
		return I2C_ERROR;
	}

	return APP_SUCCESS_OK;
}
