/*
 *****************************************************************************
 * Copyright by ams OSRAM AG                                                       *
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

//  simple tmf8806 driver

// --------------------------------------------------- includes --------------------------------

#include "tmf8806.h"
#include "tmf8806_shim.h"
#include "tof_pdrv.h"

// --------------------------------------------------- defines for HW registers --------------------------------

#define ENABLE_OFFSET 0xe0
#define ENABLE__cpu_reset__WIDTH 1
#define ENABLE__cpu_reset__SHIFT 7
#define ENABLE__cpu_reset__RESET 0
// 0
#define ENABLE__cpu_ready__WIDTH 1
#define ENABLE__cpu_ready__SHIFT 6
#define ENABLE__pinmux_unlock__WIDTH 1
#define ENABLE__pinmux_unlock__SHIFT 3
#define ENABLE__pinmux_unlock__RESET 0
#define ENABLE__stop_wait__WIDTH 1
#define ENABLE__stop_wait__SHIFT 2
#define ENABLE__stop_wait__RESET 0
// 0
#define ENABLE__waiting__WIDTH 1
#define ENABLE__waiting__SHIFT 1
#define ENABLE__pon__WIDTH 1
#define ENABLE__pon__SHIFT 0
#define ENABLE__pon__RESET 1

#define INT_STATUS_OFFSET 0xe1
#define INT_STATUS__int4__WIDTH 1
#define INT_STATUS__int4__SHIFT 3
#define INT_STATUS__int4__RESET 0
#define INT_STATUS__int3__WIDTH 1
#define INT_STATUS__int3__SHIFT 2
#define INT_STATUS__int3__RESET 0
#define INT_STATUS__int2__WIDTH 1
#define INT_STATUS__int2__SHIFT 1
#define INT_STATUS__int2__RESET 0
#define INT_STATUS__int1__WIDTH 1
#define INT_STATUS__int1__SHIFT 0
#define INT_STATUS__int1__RESET 0

#define INT_ENAB_OFFSET 0xe2
#define INT_ENAB__int4_enab__WIDTH 1
#define INT_ENAB__int4_enab__SHIFT 3
#define INT_ENAB__int4_enab__RESET 0
#define INT_ENAB__int3_enab__WIDTH 1
#define INT_ENAB__int3_enab__SHIFT 2
#define INT_ENAB__int3_enab__RESET 0
#define INT_ENAB__int2_enab__WIDTH 1
#define INT_ENAB__int2_enab__SHIFT 1
#define INT_ENAB__int2_enab__RESET 0
#define INT_ENAB__int1_enab__WIDTH 1
#define INT_ENAB__int1_enab__SHIFT 0
#define INT_ENAB__int1_enab__RESET 0

#define ID_OFFSET 0xe3
#define ID__id__WIDTH 6
#define ID__id__SHIFT 0

#define REVID_OFFSET 0xe4
#define REVID__rev_id__WIDTH 3
#define REVID__rev_id__SHIFT 0

#define RESETREASON_OFFSET 0xf0
#define RESETREASON__soft_reset__WIDTH 1
#define RESETREASON__soft_reset__SHIFT 7
#define RESETREASON__soft_reset__RESET 0
#define RESETREASON__rrsn_watchdog__WIDTH 1
#define RESETREASON__rrsn_watchdog__SHIFT 3
#define RESETREASON__rrsn_watchdog__RESET 0
#define RESETREASON__rrsn_cpu_reset__WIDTH 1
#define RESETREASON__rrsn_cpu_reset__SHIFT 2
#define RESETREASON__rrsn_cpu_reset__RESET 0
#define RESETREASON__rrsn_soft_reset__WIDTH 1
#define RESETREASON__rrsn_soft_reset__SHIFT 1
#define RESETREASON__rrsn_soft_reset__RESET 0
#define RESETREASON__rrsn_coldstart__WIDTH 1
#define RESETREASON__rrsn_coldstart__SHIFT 0
#define RESETREASON__rrsn_coldstart__RESET 0


// --------------------------------------------------- defines --------------------------------

#define TMF8806_COM_APP_ID                                  0x0   // register address
#define TMF8806_COM_APP_ID__application                     0xC0  // measurement application id
#define TMF8806_COM_APP_ID__bootloader                      0x80  // bootloader application id

#define TMF8806_CPU_IS_READY_MASK (REG_MASK(ENABLE__cpu_ready) | REG_MASK(ENABLE__pon))


// --------------------------------------------------- bootloader -----------------------------

#define TMF8806_COM_REG_SWITCH_APP                0x02  // Bootloader to switch application to ROM app
#define TMF8806_COM_CMD_STAT                      0x08  // Bootloader command Status Register

#define TMF8806_COM_CMD_STAT__bl_cmd_ok            0x00
#define TMF8806_COM_CMD_STAT__bl_cmd_errors        0x0F  // all success/error are below or equal to this number
#define TMF8806_COM_CMD_STAT__bl_cmd_ramremap      0x11  // Bootloader command to remap the vector table into RAM (Start RAM application).
#define TMF8806_COM_CMD_STAT__bl_cmd_download_init 0x14  // Bootloader command to initialise the device for firmware download
#define TMF8806_COM_CMD_STAT__bl_cmd_r_ram         0x40  // Read from BL RAM.
#define TMF8806_COM_CMD_STAT__bl_cmd_w_ram         0x41  // Write to BL RAM.
#define TMF8806_COM_CMD_STAT__bl_cmd_addr_ram      0x43  // Set address pointer in RAM for Read/Write to BL RAM.

#define TMF8806_COM_CMD_download_init_seed         0x29  // seed for download init command

#define BL_HEADER           2     // bootloader header is 2 bytes
#define BL_MAX_DATA_PAYLOAD 128   // bootloader data payload can be up to 128
#define BL_FOOTER           1     // bootloader footer is 1 byte (crc)

// Bootloader maximum wait sequences
#define BL_CMD_DOWNLOAD_INIT_TIMEOUT_MS     1
#define BL_CMD_SET_ADDR_TIMEOUT_MS          1
#define BL_CMD_W_RAM_TIMEOUT_MS             1
#define BL_CMD_RAM_REMAP_TIMEOUT_MS         1
#define BL_WAKEUP_DELAY_US                  200

// wait for version readout, to switch from ROM to RAM (and have the version published on I2C)
#define APP_PUBLISH_VERSION_WAIT_TIME_MS 10

// --------------------------------------------------- application ----------------------------

// application status, we check only for ok or accepted, everything between 2 and 15 (inclusive)
// is an error
#define TMF8806_COM_CMD_STAT__stat_ok                       0x0  // Everything is okay
#define TMF8806_COM_CMD_STAT__stat_accepted                 0x2  // Everything is okay too, send sop to halt ongoing command

// commands for histogram reading
#define TMF8806_APP_CMD_STAT__cmd_histogram_readout     0x30    // command to configure histograms for reading
#define TMF8806_APP_CMD_STAT__cmd_continue              0x32    // 1-32-bit word as configuration
#define TMF8806_APP_CMD_STAT__cmd_read_histogram        0x80    // id for histograms

// histogram dumping selection masks
#define TMF8806_APP_CMD_STAT__ec_histogram              (1UL << TMF8806_DIAG_HIST_ELECTRICAL_CAL)
#define TMF8806_APP_CMD_STAT__proximity                 (1UL << TMF8806_DIAG_HIST_PROXIMITY)
#define TMF8806_APP_CMD_STAT__distance                  (1UL << TMF8806_DIAG_HIST_DISTANCE)
#define TMF8806_APP_CMD_STAT__pileup                    (1UL << TMF8806_DIAG_HIST_ALG_PILEUP)
#define TMF8806_APP_CMD_STAT__sum                       (1UL << TMF8806_DIAG_HIST_ALG_PU_TDC_SUM)

// VCSEL is running 4x osc = 4.7MHz*4
#define TMF8806_MIN_VCSEL_CLK_FREQ_MHZ                  18

// Application maximum wait sequences
#define APP_CMD_MEASURE_TIMEOUT_MS                      5
#define APP_CMD_STOP_TIMEOUT_MS(iterations)             (5 + (iterations)/(TMF8806_MIN_VCSEL_CLK_FREQ_MHZ))
#define APP_CMD_FACTORY_CALIB_TIMEOUT_MS                5
#define APP_CMD_CLEARED_TIMEOUT_MS                      1
#define APP_CMD_READ_HISTOGRAM_TIMEOUT_MS               3

#define TMF8806_APP_STATE_ERROR                         2     // application is in state error


// ----------------------------------------------------------------------------------------------------


// clock correction pairs index calculation
#define CLK_CORRECTION_IDX_MODULO( x )    ( (x) & ( (TMF8806_CLK_CORRECTION_PAIRS)-1 ) )

// how accurate the calculation is going to be. The higher the accuracy the less far apart are
// the pairs allowed. An 8 precision means that the factor is 1/256 accurate.
#define CALC_PRECISION                                                  8

extern bool is_8806chipid_matched;

// Saturation macro for 16-bit
#define SATURATE16( v )                                                 ( (v) > 0xFFFF ? 0xFFFF : (v) )

// For TMF8806 sys ticks can be invalid if they have LSB set.
#define TMF8806_SYS_TICK_IS_VALID( tick )                               ((tick) & 1)

// how many tdcBinCalibrationQ9 values there are
#define TMF8806_NUMBER_BIN_CALIB_VALUES     5

/* Check if buffer is big enough for holding the necessary data for the driver */
#if ( ( (BL_HEADER + BL_MAX_DATA_PAYLOAD + BL_FOOTER + 1) > TMF8806_DATA_BUFFER_SIZE ) \
    || ((TMF8806_COM_FACTORY_CALIB_DATA_COMPRESSED + TMF8806_COM_STATE_DATA_COMPRESSED) > TMF8806_DATA_BUFFER_SIZE)                        \
    || ( (TMF8806_COM_RESULT__measurement_result_size) > TMF8806_DATA_BUFFER_SIZE ) )
  #error "Increase data buffer size"
#endif

// -------------------------------------------------------- constants ----------------------------------------------

const tmf8806MeasureCmd defaultConfig =
{ .data = { .spreadSpecSpadChp = { .amplitude = 0
								, .config = 0
								, .reserved = 0
								}
		, .spreadSpecVcselChp = { .amplitude = 0
								, .config = 0
								, .singleEdgeMode = 0
								, .reserved = 0
								}
		, .data = { .factoryCal = 1                               // load factory calib
					, .algState = 1                                 // load alg state
					, .reserved = 0
					, .spadDeadTime = 0                             // 0 = 97ns, 4 = 16ns, 7 = 4ns
					, .spadSelect = 1                               // all SPAD for prox
					}
		, .algo = { .reserved0 = 0
					, .distanceEnabled = 1
					, .vcselClkDiv2 = 0
					, .distanceMode = 0                             // 0=2.5m
					, .immediateInterrupt = 0
					, .reserved = 0
					, .algKeepReady = 0                             // 0 = power saving on
					}
		, .gpio = { .gpio0 = 0
					, .gpio1 = 0
					}
		, .daxDelay100us = 0
		, .snr = { .threshold = 6
					, .vcselClkSpreadSpecAmplitude = 0
					}
		, .repetitionPeriodMs = 33
		, .kIters  = 400
		, .command = TMF8806_COM_CMD_STAT__cmd_measure
		}
};

const tmf8806StateData defaultStateData =
{ .id = 2
, .reserved0 = 0
, .breakDownVoltage = 60
, .avgRefBinPosUQ9 = 0
, .calTemp = TMF8806_TEMP_INVALID
, .force20MhzVcselTemp = TMF8806_TEMP_INVALID
, .tdcBinCalibrationQ9 = { 0, 0, 0, 0, 0 }
};

const tmf8806FactoryCalibData defaultFactoryCalib =
{ .id = 2
, .crosstalkIntensity = 0
, .crosstalkTdc1Ch0BinPosUQ6Lsb = 0
, .crosstalkTdc1Ch0BinPosUQ6Msb = 0     // UQ6.6 -> (this is only the upper 4 bits of UQ6.0)
, .crosstalkTdc1Ch1BinPosDeltaQ6 = 0    // offset to TDC1
, .crosstalkTdc2Ch0BinPosDeltaQ6 = 0    // offset to TDC1
, .crosstalkTdc2Ch1BinPosDeltaQ6 = 0    // offset to TDC1
, .crosstalkTdc3Ch0BinPosDeltaQ6Lsb = 0 // offset to TDC1
, .crosstalkTdc3Ch0BinPosDeltaQ6Msb = 0
, .crosstalkTdc3Ch1BinPosDeltaQ6 = 0    // offset to TDC1
, .crosstalkTdc4Ch0BinPosDeltaQ6 = 0    // offset to TDC1
, .crosstalkTdc4Ch1BinPosDeltaQ6Lsb = 0 // offset to TDC1
, .crosstalkTdc4Ch1BinPosDeltaQ6Msb = 0
, .reserved = 0
, .opticalOffsetQ3 = 0
};

// Driver Version
const tmf8806DriverInfo tmf8806DriverInfoReset =
{ .version = { TMF8806_DRIVER_MAJOR_VERSION , TMF8806_DRIVER_MINOR_VERSION }
};

const tmf8806DeviceInfo tmf8806DeviceInfoReset =
{ .deviceSerialNumber = 0
, .appVersion = { 0, 0, 0, 0 }
, .chipVersion = { 0, 0}
};


// -------------------------------------------------------- variables ----------------------------------------------


// -------------------------------------------------------- functions ----------------------------------------------

void tmf8806ScaleAndPrintHistogram ( void * dptr, uint8_t histType, uint8_t id, uint8_t * data, uint8_t scale );
static void tmf8806ResetClockCorrection( tmf8806Driver * driver );

void tmf8806Initialise ( tmf8806Driver * driver, uint8_t logLevel )
{
	tmf8806ResetClockCorrection( driver );
	driver->i2cSlaveAddress = TMF8806_SLAVE_ADDR;
	driver->clkCorrectionEnable = 1;                  // default is on
	driver->logLevel = logLevel;
	driver->measureConfig = defaultConfig;
	driver->factoryCalib = defaultFactoryCalib;
	driver->stateData = defaultStateData;
	driver->info = tmf8806DriverInfoReset;
	driver->device = tmf8806DeviceInfoReset;
}

// Function to overwrite the default log level
void tmf8806SetLogLevel ( tmf8806Driver * driver, uint8_t level )
{
	driver->logLevel = level;
}

// Function to set clock correction on or off.
// enable ... if <>0 clock correction is enabled (default)
// enable ... if ==0 clock correction is disabled
void tmf8806ClkCorrection ( tmf8806Driver * driver, uint8_t enable )
{
	driver->clkCorrectionEnable = !!enable;
	tmf8806ResetClockCorrection( driver );
}

// Function executes a reset of the device
void tmf8806Reset ( tmf8806Driver * driver )
{
	driver->dataBuffer[0] = REG_MASK( RESETREASON__soft_reset );
	i2cTxReg( driver, driver->i2cSlaveAddress, RESETREASON_OFFSET, 1, driver->dataBuffer );
	if ( driver->logLevel >= TMF8806_LOG_LEVEL_VERBOSE )
	{
		PRINT_STR( "reset" );
		PRINT_LN( );
	}
	tmf8806ResetClockCorrection( driver );
}

// Function sets the enable PIN high
void tmf8806Enable ( tmf8806Driver * driver )
{
	enablePinHigh( driver );
	tmf8806Initialise( driver, driver->logLevel );           // when enable gets high, the HW resets to default slave addr
}

// Function clears the enable PIN (=low)
void tmf8806Disable ( tmf8806Driver * driver )
{
	enablePinLow( driver );
}

// Function checks if the CPU becomes ready within the given time
int8_t tmf8806IsCpuReady ( tmf8806Driver * driver, uint8_t waitInMs)
{
	uint16_t timeout100Us = waitInMs * 10;
	uint32_t t = getSysTick( );
	do
	{
		driver->dataBuffer[0] = 0;                                                          // clear before reading
		i2cRxReg( driver, driver->i2cSlaveAddress, ENABLE_OFFSET, 1, driver->dataBuffer );  // read the enable register to determine cpu ready

		i2cRxReg( driver, driver->i2cSlaveAddress, ID_OFFSET, 2, driver->device.chipVersion );
		if(driver->device.chipVersion[0] == TOF8806_CHIP_ID){
			is_8806chipid_matched = TRUE;
		}else{
			is_8806chipid_matched = FALSE;
		}
		CAM_EXT_INFO(CAM_EXT_TOF, "read chip id:0x%X ,is_8806chipid_matched: %d\n", driver->device.chipVersion[0],is_8806chipid_matched);
		if ( driver->dataBuffer[0] & REG_MASK(ENABLE__cpu_ready) )                          // test if CPU ready bit is set
		{
			if ( driver->logLevel >= TMF8806_LOG_LEVEL_VERBOSE )
			{
				PRINT_STR( "CPU ready" );
				PRINT_LN( );
			}
			return 1;                                               // done
		}
		else if ( timeout100Us )                                      // only wait until it is the last time we go through the loop, that would be a waste of time to wait again
		{
			delayInMicroseconds( 100 );
		}
	} while ( timeout100Us-- );
	if ( driver->logLevel >= TMF8806_LOG_LEVEL_ERROR )
	{
		t = getSysTick( ) - t;  // duration
		PRINT_STR( "#Err" );
		PRINT_CHAR( SEPARATOR );
		PRINT_STR( "CPU not ready waited" );
		PRINT_CHAR( SEPARATOR );
		PRINT_INT( t );
		PRINT_LN( );
	}
	return 0;                                                 // cpu did not get ready
}

// check if the initial shutdown after a coldstart is still in progress. if it is wait a maximum of 1.5 milliseconds for
// it to finish
#define TMF8806_SHUTDOWN_MAX_WAIT 150
static void tmf8806CheckShutdown ( tmf8806Driver * driver )
{
	uint8_t maxWait = TMF8806_SHUTDOWN_MAX_WAIT;   // 150 * 10us = 1500 usec maximum wait time for a shutdown to occur else device is waking up
	while ( ( driver->dataBuffer[0] & TMF8806_CPU_IS_READY_MASK ) == REG_MASK(ENABLE__pon) && --maxWait ) // wake-up is pending or shutdown is pending
	{
		delayInMicroseconds( 10 );
		i2cRxReg( driver, driver->i2cSlaveAddress, ENABLE_OFFSET, 1, driver->dataBuffer );    // read the enable register to dermine power state
	}
	if ( driver->logLevel >= TMF8806_LOG_LEVEL_INFO && maxWait != TMF8806_SHUTDOWN_MAX_WAIT )
	{
		PRINT_STR( "shutdown waited 10us*" );
		PRINT_INT( TMF8806_SHUTDOWN_MAX_WAIT - maxWait );
		PRINT_LN( );
	}
}

// Function attemps a wakeup of the device
void tmf8806Wakeup ( tmf8806Driver * driver )
{
	driver->dataBuffer[0] = 0;                                                            // clear before reading
	i2cRxReg( driver, driver->i2cSlaveAddress, ENABLE_OFFSET, 1, driver->dataBuffer );    // read the enable register to dermine power state
	tmf8806CheckShutdown( driver );
	if ( ( driver->dataBuffer[0] & TMF8806_CPU_IS_READY_MASK ) != TMF8806_CPU_IS_READY_MASK )
	{
		driver->dataBuffer[0] = REG_MASK(ENABLE__pon) ;        // PON=1
		i2cTxReg( driver, driver->i2cSlaveAddress, ENABLE_OFFSET, 1, driver->dataBuffer );  // set PON bit in enable register
		if ( driver->logLevel >= TMF8806_LOG_LEVEL_VERBOSE )
		{
			PRINT_STR( "PON=1" );
			PRINT_LN( );
		}
	}
	else
	{
		if ( driver->logLevel >= TMF8806_LOG_LEVEL_VERBOSE )
		{
			PRINT_STR( "awake ENABLE=0x" );
			PRINT_UINT_HEX( driver->dataBuffer[0] );
			PRINT_LN( );
		}
	}
}

// Function puts the device in standby state
void tmf8806Standby ( tmf8806Driver * driver )
{
	driver->dataBuffer[0] = 0;                                                      // clear before reading
	i2cRxReg( driver, driver->i2cSlaveAddress, ENABLE_OFFSET, 1, driver->dataBuffer );   // read the enable register to determine power state
	if ( ( driver->dataBuffer[0] & REG_MASK(ENABLE__cpu_ready) ) != 0 )
	{
		driver->dataBuffer[0] = driver->dataBuffer[0] & ~REG_MASK(ENABLE__pon);               // clear only the PON bit
		i2cTxReg( driver, driver->i2cSlaveAddress, ENABLE_OFFSET, 1, driver->dataBuffer ); // clear PON bit in enable register
		if ( driver->logLevel >= TMF8806_LOG_LEVEL_VERBOSE )
		{
			PRINT_STR( "PON=0" );
			PRINT_LN( );
		}
	}
	else
	{
		if ( driver->logLevel >= TMF8806_LOG_LEVEL_VERBOSE )
		{
			PRINT_STR( "standby ENABLE=0x" );
			PRINT_UINT_HEX( driver->dataBuffer[0] );
			PRINT_LN( );
		}
	}
}

// function to check if a register has a specific value
static int8_t tmf8806CheckRegister ( tmf8806Driver * driver, uint8_t regAddr, uint8_t expected, uint8_t len, uint16_t timeoutInMs )
{
	int8_t res = APP_ERROR_TIMEOUT;
	uint8_t i;
	uint32_t timeout100Us = timeoutInMs * 10;
	uint32_t t = getSysTick();
	do
	{
		driver->dataBuffer[0] = ~expected;
		i2cRxReg( driver, driver->i2cSlaveAddress, regAddr, len, driver->dataBuffer );
		if ( driver->dataBuffer[0] == expected )
		{
			return APP_SUCCESS_OK;                        // early exit on success
		}
	  else if ( timeout100Us )                             // do not wait if timeout is 0
		{
			delayInMicroseconds(100);
		}
	} while ( timeout100Us-- > 0 );
	if ( driver->logLevel >= TMF8806_LOG_LEVEL_ERROR )
	{
		t = getSysTick() - t;  // duration
		PRINT_STR( "#Err" );
		PRINT_CHAR( SEPARATOR );
		PRINT_INT( res );
		PRINT_CHAR( SEPARATOR );
		PRINT_STR( "waited " );
		PRINT_INT( t );
		PRINT_CHAR( SEPARATOR );
		PRINT_STR( " reg=0x" );
		PRINT_UINT_HEX( regAddr );
		for ( i = 0; i < len; i++ )
		{
			PRINT_STR( " 0x" );
			PRINT_UINT_HEX( driver->dataBuffer[i] );
		}
		PRINT_LN( );
	}
	return res;        // return error (from driver - not application specific error)
}


// --------------------------------------- bootloader ------------------------------------------

// calculate the checksum according to bootloader spec
static uint8_t tmf8806BootloaderChecksum ( uint8_t * data, uint8_t len )
{
	uint8_t sum = 0;
	while ( len-- > 0 )
	{
		sum += *data;
		data++;
	}
	sum = sum ^ 0xFF;
	return sum;
}
// execute command to init download
static int8_t tmf8806BootloaderDownloadInit ( tmf8806Driver * driver )
{
	driver->dataBuffer[0] = TMF8806_COM_CMD_STAT__bl_cmd_download_init;
	driver->dataBuffer[1] = 1;
	driver->dataBuffer[2] = TMF8806_COM_CMD_download_init_seed;
	driver->dataBuffer[3] = tmf8806BootloaderChecksum( driver->dataBuffer, 3 );
	i2cTxReg( driver, driver->i2cSlaveAddress, TMF8806_COM_CMD_STAT, 4, driver->dataBuffer );
	return tmf8806CheckRegister( driver, TMF8806_COM_CMD_STAT, TMF8806_COM_CMD_STAT__bl_cmd_ok, 3, BL_CMD_DOWNLOAD_INIT_TIMEOUT_MS );      // many BL errors only have 3 bytes
}

// execute command to set the RAM address pointer for RAM read/writes
static int8_t tmf8806BootloaderSetRamAddr ( tmf8806Driver * driver, uint16_t addr )
{
	driver->dataBuffer[0] = TMF8806_COM_CMD_STAT__bl_cmd_addr_ram;
	driver->dataBuffer[1] = 2;
	tmf8806SetUint16( addr, &(driver->dataBuffer[2]) );
	driver->dataBuffer[4] = tmf8806BootloaderChecksum( driver->dataBuffer, 4 );
	i2cTxReg( driver, driver->i2cSlaveAddress, TMF8806_COM_CMD_STAT, 5, driver->dataBuffer );
	return tmf8806CheckRegister( driver, TMF8806_COM_CMD_STAT, TMF8806_COM_CMD_STAT__bl_cmd_ok, 3, BL_CMD_SET_ADDR_TIMEOUT_MS );      // many BL errors only have 3 bytes
}

// execute command to write a chunk of data to RAM
static int8_t tmf8806BootloaderWriteRam ( tmf8806Driver * driver, uint8_t len )
{
	driver->dataBuffer[0] = TMF8806_COM_CMD_STAT__bl_cmd_w_ram;
	driver->dataBuffer[1] = len;
	driver->dataBuffer[BL_HEADER+len] = tmf8806BootloaderChecksum( driver->dataBuffer, BL_HEADER+len );
	i2cTxReg( driver, driver->i2cSlaveAddress, TMF8806_COM_CMD_STAT, BL_HEADER+len+BL_FOOTER, driver->dataBuffer );
	return tmf8806CheckRegister( driver, TMF8806_COM_CMD_STAT,TMF8806_COM_CMD_STAT__bl_cmd_ok, 3, BL_CMD_W_RAM_TIMEOUT_MS );    // many BL errors only have 3 bytes
}

// execute command RAM remap to address 0 and continue running from RAM
static int8_t tmf8806BootloaderRamRemap ( tmf8806Driver * driver, uint8_t appId )
{
	int8_t stat;
	driver->dataBuffer[0] = TMF8806_COM_CMD_STAT__bl_cmd_ramremap;
	driver->dataBuffer[1] = 0;
	driver->dataBuffer[BL_HEADER] = tmf8806BootloaderChecksum( driver->dataBuffer, BL_HEADER );
	i2cTxReg( driver, driver->i2cSlaveAddress, TMF8806_COM_CMD_STAT, BL_HEADER+BL_FOOTER, driver->dataBuffer );
	delayInMicroseconds( APP_PUBLISH_VERSION_WAIT_TIME_MS * 1000 );
	// ram remap -> the bootloader will not answer to this command if successfull, so check the application id register instead
	stat = tmf8806CheckRegister( driver, TMF8806_COM_APP_ID, appId, 1, BL_CMD_RAM_REMAP_TIMEOUT_MS );   // all tmf8806 apps have an app_id
	tmf8806ReadDeviceInfo( driver );                                                                    // read the app specific info always
	return stat;
}

// download the image file to RAM
int8_t tmf8806DownloadFirmware ( tmf8806Driver * driver, uint32_t imageStartAddress, const uint8_t * image, int32_t imageSizeInBytes )
{
	int32_t idx;
	int8_t stat = BL_SUCCESS_OK;
	uint8_t chunkLen = 0;

	if ( driver->logLevel >= TMF8806_LOG_LEVEL_VERBOSE )
	{
		PRINT_STR( "Image addr=0x" );
		PRINT_UINT_HEX( PTR_TO_UINT(image) );
		PRINT_STR( " len=" );
		PRINT_INT( imageSizeInBytes );

		for ( idx = 0; idx < 16; idx++ )
		{
			uint8_t d = readProgramMemoryByte( (uint64_t)(image + idx) );
			PRINT_STR( " 0x" );
			PRINT_UINT_HEX( d );      // read from program memory space
		}

		PRINT_LN( );
	}
	stat = tmf8806BootloaderDownloadInit( driver );
	if ( stat == BL_SUCCESS_OK )
	{
		stat = tmf8806BootloaderSetRamAddr( driver, imageStartAddress );
	  idx = 0;  // start again at the image begin
		while ( stat == BL_SUCCESS_OK && idx < imageSizeInBytes )
		{
			if ( driver->logLevel >= TMF8806_LOG_LEVEL_VERBOSE )
			{
				PRINT_STR( "Dwnld addr=0x" );
				PRINT_UINT_HEX( (uint32_t)idx );
				PRINT_LN( );
			}
			for( chunkLen = 0; chunkLen < BL_MAX_DATA_PAYLOAD && idx < imageSizeInBytes; chunkLen++, idx++ )
			{
				driver->dataBuffer[BL_HEADER + chunkLen] = readProgramMemoryByte( (uint64_t)(image + idx) );              // read from code memory into local ram buffer
				if ( driver->logLevel >= TMF8806_LOG_LEVEL_INFO )
				{
					if ( ( idx & 0x7F) == 0x7F )
					{
						PRINT_CHAR( '.' ); // always show progress - download can be slow on MCU
					}
				}
			}
			stat = tmf8806BootloaderWriteRam( driver, chunkLen );
		}
	}
	if ( stat == BL_SUCCESS_OK )
	{
		if ( driver->logLevel >= TMF8806_LOG_LEVEL_INFO )
		{
			PRINT_LN( );
		}
		stat = tmf8806BootloaderRamRemap( driver, TMF8806_COM_APP_ID__application );      // if you load a test-application this may have another ID
		if ( stat == BL_SUCCESS_OK )
		{
			if ( driver->logLevel >= TMF8806_LOG_LEVEL_INFO )
			{
				PRINT_STR( "FW dwnld" );
				PRINT_LN( );
			}
			return stat;
		}
	}
	if ( driver->logLevel >= TMF8806_LOG_LEVEL_ERROR )
	{
		PRINT_STR( "#Err" );
		PRINT_CHAR( SEPARATOR );
		PRINT_STR( "FW init, dwnld or REMAP" );
		PRINT_LN( );
	}
	return stat;
}

int8_t tmf8806SwitchToRomApplication ( tmf8806Driver * driver )
{
	int8_t stat;
	driver->dataBuffer[0] = TMF8806_COM_APP_ID__application;
	i2cTxReg( driver, driver->i2cSlaveAddress, TMF8806_COM_REG_SWITCH_APP, 1, driver->dataBuffer );
	delayInMicroseconds( APP_PUBLISH_VERSION_WAIT_TIME_MS * 1000 );
	// ram remap ->  check the application id register instead
	stat = tmf8806CheckRegister( driver, TMF8806_COM_APP_ID, TMF8806_COM_APP_ID__application, 1, BL_CMD_RAM_REMAP_TIMEOUT_MS );   // all tmf8806 apps have an app_id
	tmf8806ReadDeviceInfo( driver );                                                                    // read the app specific info always
	return stat;

}
// convenience functions -----------------------------------------------------

// Convert 4 bytes in little endian format into an uint32_t
uint32_t tmf8806GetUint32 ( const uint8_t * data )
{
	uint32_t t = data[ 3 ];
	t = (t << 8 ) + data[ 2 ];
	t = (t << 8 ) + data[ 1 ];
	t = (t << 8 ) + data[ 0 ];
	return t;
}

// Convert uint32_t in little endian format to 4 bytes
void tmf8806SetUint32 ( uint32_t value, uint8_t * data )
{
	data[ 0 ] = value;
	data[ 1 ] = value >> 8;
	data[ 2 ] = value >> 16;
	data[ 3 ] = value >> 24;
}

// Convert 2 bytes in little endian format into an uint16_t
uint16_t tmf8806GetUint16 ( const uint8_t * data )
{
	uint16_t t = data[ 1 ];
	t = (t << 8 ) + data[ 0 ];
	return t;
}

// Convert uint16_t in little endian format to 2 bytes
void tmf8806SetUint16 ( uint16_t value, uint8_t * data )
{
	data[ 0 ] = value;
	data[ 1 ] = value >> 8;
}

// --------------------------------------- application -----------------------------------------

// Reset clock correction calculation
static void tmf8806ResetClockCorrection ( tmf8806Driver * driver )
{
	uint8_t i;
	driver->clkCorrectionIdx = 0;                      // reset clock correction
	driver->clkCorrRatioUQ = (1<<15);                  // this is 1.0 in UQ1.15
	for ( i = 0; i < TMF8806_CLK_CORRECTION_PAIRS; i++ )
	{
		driver->hostTicks[ i ] = 0;
		driver->tmf8806Ticks[ i ] = 0;                  // initialise the tmf8828Ticks to a value that has the LSB cleared -> can identify that these are no real ticks
	}
	if ( driver->logLevel & TMF8806_LOG_LEVEL_CLK_CORRECTION )
	{
		PRINT_STR( "ClkCorr reset" );
		PRINT_LN( );
	}
}

// Add a host tick and a tmf8806 tick to the clock correction list.
static void tmf8806ClockCorrectionAddPair ( tmf8806Driver * driver, uint32_t hostTick, uint32_t tmf8806Tick )
{
	if ( TMF8806_SYS_TICK_IS_VALID( tmf8806Tick ) )                             // only use valid ticks if tmf8806Tick
	{
		driver->clkCorrectionIdx = CLK_CORRECTION_IDX_MODULO( driver->clkCorrectionIdx + 1 );     // increment and take care of wrap-over
		driver->hostTicks[ driver->clkCorrectionIdx ] = hostTick;
		driver->tmf8806Ticks[ driver->clkCorrectionIdx ] = tmf8806Tick;
		if ( driver->logLevel >= TMF8806_LOG_LEVEL_VERBOSE )
		{
			PRINT_STR( "#ClkCorr" );
			PRINT_CHAR( SEPARATOR );
			PRINT_INT( hostTick );
			PRINT_CHAR( SEPARATOR );
			PRINT_INT( tmf8806Tick );
			PRINT_LN( );
		}
	}
	else if ( driver->logLevel & TMF8806_LOG_LEVEL_CLK_CORRECTION )
	{
		PRINT_STR( "ClkCorr ticks invalid " );      // this can happen if the host did read out the data very, very fast,
		PRINT_INT( tmf8806Tick );                   // and the device was busy handling other higher priority interrupts
		PRINT_LN( );                                // The device does always set the LSB of the sys-tick to indicate that
	}                                             // the device did set the sys-tick.
}

// function to check if command is accepted within time
static int8_t tmf8806AppCheckCommandDone ( tmf8806Driver * driver, uint8_t expected, uint16_t timeoutInMs )
{
	uint32_t t;
	int8_t res = APP_ERROR_TIMEOUT;
	uint8_t i;
	uint32_t timeout100Us = timeoutInMs * 10;
	t = getSysTick();                                       // wait start timestamp
	do
	{
		driver->dataBuffer[0] = ~TMF8806_COM_CMD_STAT__stat_ok;
		driver->dataBuffer[1] = ~expected;                                                                  // prev is 2nd read register
		i2cRxReg( driver, driver->i2cSlaveAddress, TMF8806_COM_CMD_REG, 2, driver->dataBuffer );            // read cmd + prev register
		if ( driver->dataBuffer[1] == expected && driver->dataBuffer[0] == TMF8806_COM_CMD_STAT__stat_ok )  // all commands must be cleared within timeout time
		{
			if ( driver->logLevel >= TMF8806_LOG_LEVEL_VERBOSE )
			{
				t = getSysTick() - t;   // duration
				PRINT_STR( "#Cmd" );
				PRINT_CHAR( SEPARATOR );
				PRINT_INT( expected );
				PRINT_CHAR( SEPARATOR );
				PRINT_STR( "waited" );
				PRINT_CHAR( SEPARATOR );
				PRINT_INT( t );
				PRINT_LN( );
			}
			return APP_SUCCESS_OK;                        // early exit on success
		}
		else  // not both prev and cmd register have expected value, check state register
		{
			i2cRxReg( driver, driver->i2cSlaveAddress, TMF8806_COM_STATE, 2, &(driver->dataBuffer[2]) );      // keep values of cmd and prev for logging
			if ( driver->dataBuffer[2] == TMF8806_APP_STATE_ERROR )
			{
				res = APP_ERROR_CMD;    // driver error code, device error code is only logged
				timeout100Us = 0;        // early exit in error case
			}
			if ( timeout100Us )        // do not wait if timeout is 0
			{
				delayInMicroseconds( 100 );
			}
		}
	} while ( timeout100Us-- > 0 );
	if ( driver->logLevel >= TMF8806_LOG_LEVEL_ERROR )
	{
		t = getSysTick() - t;    // duration
		PRINT_STR( "#Err" );
		PRINT_CHAR( SEPARATOR );
		PRINT_INT( res );
		PRINT_CHAR( SEPARATOR );
		PRINT_STR( "waited" );
		PRINT_CHAR( SEPARATOR );
		PRINT_INT( t );
		PRINT_CHAR( SEPARATOR );
		PRINT_STR( "exp" );
		PRINT_CHAR( SEPARATOR );
		PRINT_INT( expected );
		PRINT_CHAR( SEPARATOR );
		for ( i = 0; i < 4; i++ )     // print the 2x 2-bytes read
		{
			PRINT_STR( " 0x" );
			PRINT_UINT_HEX( driver->dataBuffer[i] );
		}
		PRINT_LN( );
	}
	return res;        // return error (from driver - not application specific error)
}

// function reads complete device information from the tmf8806
int8_t tmf8806ReadDeviceInfo ( tmf8806Driver * driver )
{
	int8_t res = APP_ERROR_CMD;
	driver->device = tmf8806DeviceInfoReset;
	i2cRxReg( driver, driver->i2cSlaveAddress, ID_OFFSET, 2, driver->device.chipVersion );
	i2cRxReg( driver, driver->i2cSlaveAddress, TMF8806_COM_APP_ID, 2, driver->device.appVersion );  // tmf8806 application has 2 version bytes
	if ( driver->device.appVersion[0] == TMF8806_COM_APP_ID__application )
	{
		i2cRxReg( driver, driver->i2cSlaveAddress, TMF8806_COM_MINOR_VERSION, 2, &(driver->device.appVersion[2]) ); // read minor and patch (invalid)
		driver->dataBuffer[0] = TMF8806_COM_CMD_STAT__cmd_read_serial_number;
		i2cTxReg( driver, driver->i2cSlaveAddress, TMF8806_COM_CMD_REG, 1, driver->dataBuffer );
		res = tmf8806AppCheckCommandDone( driver, TMF8806_COM_CMD_STAT__cmd_read_serial_number, APP_CMD_CLEARED_TIMEOUT_MS );
		if ( res == APP_SUCCESS_OK )
		{
	// serial number at register 0x28 .. 0x2B, read from 0x1E to 0x2B -> 14 bytes
			i2cRxReg( driver, driver->i2cSlaveAddress, TMF8806_COM_CONTENT, 14, driver->dataBuffer );
			if ( driver->dataBuffer[0] == TMF8806_COM_CMD_STAT__cmd_read_serial_number )
			{
				driver->device.deviceSerialNumber = tmf8806GetUint32( &(driver->dataBuffer[10]) ); // 0x1e + 10d = 0x28 -> first register of serial number
				res = APP_SUCCESS_OK;
			}
			else
			{
				res = APP_ERROR_NO_SERIAL_NUMBER;
			}
		}
	} // else bootloader -> has only 2 version bytes
	return res;
}

// function sets config data and optional state data and factory calibration data
// and executes the command
static int8_t tmf8806ConfigureAndExecute ( tmf8806Driver * driver, uint8_t cmd, uint16_t kIter, uint16_t timeoutInMs )
{
	int8_t res;
	uint8_t * ptr = driver->dataBuffer;
	uint8_t addCfgSize = 0;
	uint16_t kIterSave = driver->measureConfig.data.kIters;
	uint8_t cmdSave = driver->measureConfig.data.command;
	driver->measureConfig.data.kIters = kIter;                                 // save to restore later
	driver->measureConfig.data.command = cmd;                                // save to restore later
	if ( driver->measureConfig.data.data.factoryCal )
	{
		tmf8806SerializeFactoryCalibration( &(driver->factoryCalib), ptr );
		ptr += sizeof( tmf8806FactoryCalibData );
		addCfgSize += sizeof( tmf8806FactoryCalibData );
	}
	if ( driver->measureConfig.data.data.algState )
	{
		tmf8806SerializeStateData( &(driver->stateData), ptr );
		addCfgSize += sizeof( tmf8806StateData );
	}
	if ( addCfgSize )       // transfer additional configuration first
	{
		i2cTxReg( driver, driver->i2cSlaveAddress, TMF8806_COM_CONFIG, addCfgSize, driver->dataBuffer );
	}
	// configuration contains as last byte the command itself
	tmf8806SerializeConfiguration( &(driver->measureConfig), driver->dataBuffer );
	i2cTxReg( driver, driver->i2cSlaveAddress, TMF8806_COM_CMD_REG - (sizeof(tmf8806MeasureCmd) - 1), sizeof(tmf8806MeasureCmd), driver->dataBuffer );
	res = tmf8806AppCheckCommandDone( driver, cmd, timeoutInMs );
	driver->measureConfig.data.kIters = kIterSave;                     // restore
	driver->measureConfig.data.command = cmdSave;                     // restore
	return res;
}

// Function to execute a factory calibration, FC is still running when this function returns.
// check of fc done has to be done with polling the interrupt status register or waiting for
// the INT pin to get asserted
int8_t tmf8806FactoryCalibration ( tmf8806Driver * driver, uint16_t kIters )
{
	int8_t res;
	uint8_t loadFc = driver->measureConfig.data.data.factoryCal;
	uint8_t loadState = driver->measureConfig.data.data.algState;
	driver->measureConfig.data.data.factoryCal = 0;
	driver->measureConfig.data.data.algState = 0;
	res = tmf8806ConfigureAndExecute( driver, TMF8806_COM_CMD_STAT__cmd_factory_calibration, kIters, APP_CMD_FACTORY_CALIB_TIMEOUT_MS );
	driver->measureConfig.data.data.factoryCal = loadFc;
	driver->measureConfig.data.data.algState = loadState;
	return res;
}

// Function to read the factory calibration data from the i2c
int8_t tmf8806ReadFactoryCalibration ( tmf8806Driver * driver )
{
	int8_t res = APP_SUCCESS_OK;
	uint8_t len = (TMF8806_COM_CONFIG-TMF8806_COM_CONTENT) + sizeof( tmf8806FactoryCalibData );             // read in content register and factory calib data
	i2cRxReg( driver, driver->i2cSlaveAddress, TMF8806_COM_CONTENT, len, driver->dataBuffer );
	if ( driver->dataBuffer[0] == TMF8806_COM_CMD_STAT__cmd_factory_calibration )                                       // check that this is really factory calib data
	{
		tmf8806DeserializeFactoryCalibration( &(driver->dataBuffer[(TMF8806_COM_CONFIG-TMF8806_COM_CONTENT)]), &(driver->factoryCalib) );  // update driver factory calib copy
	}
	else
	{
	  res = APP_ERROR_NO_FACTORY_CALIB_PAGE;  // content is not factory calib data
	}
	return res;
}

// Function to set calibration data from host side
void tmf8806SetFactoryCalibration ( tmf8806Driver * driver, const tmf8806FactoryCalibData * calibData )
{
	driver->factoryCalib = *calibData;
}

// Function to set state data
void tmf8806SetStateData ( tmf8806Driver * driver, const tmf8806StateData * stateData )
{
	driver->stateData = *stateData;
}

// Function to set configuration
void tmf8806SetConfiguration ( tmf8806Driver * driver, const tmf8806MeasureCmd * config )
{
	driver->measureConfig = *config;
}

// function starts a measurement
int8_t tmf8806StartMeasurement ( tmf8806Driver * driver )
{
	return tmf8806ConfigureAndExecute( driver, TMF8806_COM_CMD_STAT__cmd_measure, driver->measureConfig.data.kIters, APP_CMD_MEASURE_TIMEOUT_MS );
}

// function stops a measurement
int8_t tmf8806StopMeasurement ( tmf8806Driver * driver )
{
	driver->dataBuffer[0] = TMF8806_COM_CMD_STAT__cmd_stop;
	i2cTxReg( driver, driver->i2cSlaveAddress, TMF8806_COM_CMD_REG, 1, driver->dataBuffer );
	// Stop command time is related to the number of iterations, as it may come just right after a new measurement with kIters has
	// started. So the wait time is calculated with kIters.
	return tmf8806AppCheckCommandDone( driver, TMF8806_COM_CMD_STAT__cmd_stop, APP_CMD_STOP_TIMEOUT_MS(driver->measureConfig.data.kIters) );
}

// function reads and clears the specified interrupts
uint8_t tmf8806GetAndClrInterrupts ( tmf8806Driver * driver, uint8_t mask )
{
	uint8_t setInterrupts;
	driver->dataBuffer[0] = 0;
	i2cRxReg( driver, driver->i2cSlaveAddress, INT_STATUS_OFFSET, 1, driver->dataBuffer ); // read interrupt status register
	setInterrupts = driver->dataBuffer[0] & mask;
	if ( setInterrupts )
	{
		driver->dataBuffer[0] = setInterrupts;           // clear only those that were set when we read the register, and only those we want to know
		i2cTxReg( driver, driver->i2cSlaveAddress, INT_STATUS_OFFSET, 1, driver->dataBuffer ); // clear interrupts by pushing a 1 to status register
	}
	return setInterrupts;
}

// function clears and enables the specified interrupts
void tmf8806ClrAndEnableInterrupts ( tmf8806Driver * driver, uint8_t mask )
{
	driver->dataBuffer[0] = mask;                                                          // clear all interrupts
	i2cTxReg( driver, driver->i2cSlaveAddress, INT_STATUS_OFFSET, 1, driver->dataBuffer ); // clear interrupts by pushing a 1 to status register
	driver->dataBuffer[0] = 0;
	i2cRxReg( driver, driver->i2cSlaveAddress, INT_ENAB_OFFSET, 1, driver->dataBuffer );   // read current enabled interrupts
	driver->dataBuffer[0] = driver->dataBuffer[0] | mask;                                  // enable those in the mask, keep the others if they were enabled
	i2cTxReg( driver, driver->i2cSlaveAddress, INT_ENAB_OFFSET, 1, driver->dataBuffer );
}

// function disables the specified interrupts
void tmf8806DisableAndClrInterrupts ( tmf8806Driver * driver, uint8_t mask )
{
	driver->dataBuffer[0] = 0;
	i2cRxReg( driver, driver->i2cSlaveAddress, INT_ENAB_OFFSET, 1, driver->dataBuffer );   // read current enabled interrupts
	driver->dataBuffer[0] = driver->dataBuffer[0] & ~mask;                                 // clear only those in the mask, keep the others if they were enabled
	i2cTxReg( driver, driver->i2cSlaveAddress, INT_ENAB_OFFSET, 1, driver->dataBuffer );
	driver->dataBuffer[0] = mask;
	i2cTxReg( driver, driver->i2cSlaveAddress, INT_STATUS_OFFSET, 1, driver->dataBuffer ); // clear interrupts by pushing a 1 to status register
}

// function reads the result page (if there is none the function returns an error, else success)
int8_t tmf8806ReadResult ( tmf8806Driver * driver, tmf8806DistanceResultFrame * result )
{
	uint32_t hTick;
	driver->dataBuffer[0] = 0;
	hTick = getSysTick( );                                                        // get the sys-tick just before the I2C rx
	i2cRxReg( driver, driver->i2cSlaveAddress, TMF8806_COM_STATE, TMF8806_COM_RESULT__measurement_result_size, driver->dataBuffer );
	if ( driver->dataBuffer[RESULT_REG(CONTENT)] == TMF8806_COM_RESULT__measurement_result )
	{
		uint32_t tTick = tmf8806GetUint32( driver->dataBuffer + RESULT_REG( SYS_TICK_0 ) );
		tmf8806ClockCorrectionAddPair( driver, hTick, tTick );
		tmf8806DeserializeResultFrame( &(driver->dataBuffer[RESULT_REG(RESULT_NUMBER)]), result ); // unpack byte-stream to frame
		tmf8806DeserializeStateData( result->stateData, &(driver->stateData) );      // update driver internal state data copy
		return APP_SUCCESS_OK;
	}
	return APP_ERROR_NO_RESULT_PAGE;
}

#define MAX_UINT_VALUE                  (0x80000000)
// function calculates the ratio between hDiff and tDiff in Uq1.15
static uint16_t tmf8806CalcClkRatioUQ16 ( uint32_t hDiff, uint32_t tDiff, uint16_t prevRatioUQ )
{
	uint32_t ratioUQ = prevRatioUQ;
	if ((hDiff >= HOST_TICKS_PER_10_US)
		&& (tDiff >= TMF8806_TICKS_PER_10_US)
		&& (hDiff < tDiff )                  // if this condition is wrong than the tmf8806 has wrapped over but host not
		)
	{ /* move both values to be as big as possible to increase precision to max. possible */
		while ( hDiff < MAX_UINT_VALUE && tDiff < MAX_UINT_VALUE )
		{
			hDiff <<= 1;
			tDiff <<= 1;
		}
		tDiff = tDiff / TMF8806_TICKS_PER_10_US;
		hDiff = hDiff / HOST_TICKS_PER_10_US;
		while ( hDiff < MAX_UINT_VALUE && tDiff < MAX_UINT_VALUE )      /* scale up again */
		{
			hDiff <<= 1;
			tDiff <<= 1;
		}
		tDiff = ( tDiff + (1<<14)) >> 15;    /* The number of shifts defines the number of "digits" after the decimal point. UQ1.15 range is [0..2), 1<<15=32768==1.0 */
		if ( tDiff )                         /* this can get 0 if the value was close to 2^32 because of the adding of 2^14 */
		{
			ratioUQ = (hDiff + (tDiff>>1)) / tDiff;                                       /* round the ratio */

			/* ratioUQ16 is the range of [0..2), restrict the ratioUQ to 0.5..1.5 */
			if ( ( ratioUQ > (1<<15)+(1<<14) ) || ( ratioUQ < (1<<15)-(1<<14) ) ) // this check ensures that the new value fits in 16-bit also.
			{
				ratioUQ = prevRatioUQ;
			}
		}
	}
	return (uint16_t)ratioUQ;           // return an UQ1.15 = [0..2)
}

// Correct the distance based on the clock correction pairs
uint16_t tmf8806CorrectDistance ( tmf8806Driver * driver, uint16_t distance )
{
	if ( driver->clkCorrectionEnable )
	{
		uint32_t d = distance;
		uint8_t idx = driver->clkCorrectionIdx;                                                 // last inserted
		uint8_t idx2 = CLK_CORRECTION_IDX_MODULO( idx + TMF8806_CLK_CORRECTION_PAIRS - 1 );     // oldest available
		if ( TMF8806_SYS_TICK_IS_VALID( driver->tmf8806Ticks[ idx ] ) && TMF8806_SYS_TICK_IS_VALID( driver->tmf8806Ticks[ idx2 ] ) )    // only do a correction if both tmf8806 ticks are valid
		{
			uint32_t hDiff = driver->hostTicks[ idx ] - driver->hostTicks[ idx2 ];
			uint32_t tDiff = driver->tmf8806Ticks[ idx ] - driver->tmf8806Ticks[ idx2 ];
			driver->clkCorrRatioUQ = tmf8806CalcClkRatioUQ16( hDiff, tDiff, driver->clkCorrRatioUQ );
		} /* else use last valid clock correction Ration UQ */
		d = ( driver->clkCorrRatioUQ * d + (1<<14) ) >> 15;
		// if ( driver->logLevel & TMF8806_LOG_LEVEL_CLK_CORRECTION )
		// {
		//	 PRINT_LN( );
		//	 PRINT_CHAR( '?' );
		//	 PRINT_UINT( hDiff * TMF8806_TICKS_PER_10_US );	// normalize to compare
		//	 PRINT_CHAR( ' ' );
		//	 PRINT_UINT( tDiff * HOST_TICKS_PER_10_US );		 // normalize to compare
		//	 PRINT_CHAR( ' ' );
		//	 PRINT_UINT( driver->clkCorrRatioUQ );
		//	 PRINT_CHAR( ' ' );
		//	 PRINT_UINT( distance );
		//	 PRINT_CHAR( ' ' );
		//	 PRINT_UINT( d );
		//	 PRINT_CHAR( '?' );
		//	 PRINT_LN( );
		// }
		distance = SATURATE16( d );
	}
	return distance;
}

int8_t tmf8806ConfigureHistograms ( tmf8806Driver * driver, uint8_t histograms )
{
	uint32_t d = 0;
	d = d + ( histograms & TMF8806_DUMP_HIST_ELECTRICAL_CAL ? TMF8806_APP_CMD_STAT__ec_histogram: 0 );
	d = d + ( histograms & TMF8806_DUMP_HIST_PROXIMITY ? TMF8806_APP_CMD_STAT__proximity: 0 );
	d = d + ( histograms & TMF8806_DUMP_HIST_DISTANCE ? TMF8806_APP_CMD_STAT__distance: 0 );
	d = d + ( histograms & TMF8806_DUMP_HIST_ALG_PILEUP ? TMF8806_APP_CMD_STAT__pileup: 0 );
	d = d + ( histograms & TMF8806_DUMP_HIST_ALG_PU_TDC_SUM ? TMF8806_APP_CMD_STAT__sum: 0 );
	tmf8806SetUint32( d, driver->dataBuffer );
	driver->dataBuffer[ 4 ] = TMF8806_APP_CMD_STAT__cmd_histogram_readout;
	i2cTxReg( driver, driver->i2cSlaveAddress, TMF8806_COM_CMD_REG - 4, 5, driver->dataBuffer );
	return tmf8806AppCheckCommandDone( driver, TMF8806_APP_CMD_STAT__cmd_histogram_readout, APP_CMD_CLEARED_TIMEOUT_MS );
}

static uint8_t tmf8806ReadSingleHistogram ( tmf8806Driver * driver, uint8_t id )
{
	uint8_t * data;
	uint8_t header[4];
	uint8_t htype;
	uint8_t scale = 0;
	i2cRxReg( driver, driver->i2cSlaveAddress, TMF8806_COM_DIAG_INFO, 2, header );
	htype = ( header[0] >> 1 ) & 0x1F;      // histogram type is encoded in the diagnostic info in the first byte in bits [5:1]
	if ( htype == TMF8806_DIAG_HIST_ALG_PILEUP )
	{
		scale = 1;                            // pileup histograms have to be scaled by a fixed value
	}
	else if ( htype == TMF8806_DIAG_HIST_ALG_PU_TDC_SUM )
	{
		scale = 2;                            // sum histograms have to be scaled by a fixed value
	}
	for ( uint8_t i = 0; i < 4; i++ )       // a single histogram consists of 4 chunks
	{
		i2cRxReg( driver, driver->i2cSlaveAddress, TMF8806_COM_STATE, 4, header );
		if ( header[2] == id+i )
		{
			data = &( driver->dataBuffer[ (i & 1) * TMF8806_HISTOGRAM_BINS ] );
			tmf8806ReadQuadHistogram( driver, driver->i2cSlaveAddress, data );
			if ( i == 1 || i == 3 )
			{
				if ( htype != TMF8806_DIAG_HIST_ALG_PILEUP && htype != TMF8806_DIAG_HIST_ALG_PU_TDC_SUM )
				{
					scale = driver->dataBuffer[ (TMF8806_HISTOGRAM_BINS - 1) * 2 ];      // scale (shift) factor is encoded in the lower byte of the last bin
				}
				tmf8806ScaleAndPrintHistogram( driver, htype, id+i, driver->dataBuffer, scale ); // scale and print histogram
			}
		}
	}
	return htype;
}

int8_t tmf8806ReadHistograms ( tmf8806Driver * driver )
{
	uint8_t hType;
	int8_t res = APP_SUCCESS_OK;
	uint8_t id = TMF8806_APP_CMD_STAT__cmd_read_histogram;
	driver->dataBuffer[ 0 ] = TMF8806_APP_CMD_STAT__cmd_read_histogram;
	i2cTxReg( driver, driver->i2cSlaveAddress, TMF8806_COM_CMD_REG, 1, driver->dataBuffer );
	res = tmf8806AppCheckCommandDone( driver, TMF8806_APP_CMD_STAT__cmd_read_histogram, APP_CMD_READ_HISTOGRAM_TIMEOUT_MS );
	if ( res == APP_SUCCESS_OK )
	{
		hType = tmf8806ReadSingleHistogram( driver, id );
		if ( hType == TMF8806_DIAG_HIST_DISTANCE || hType == TMF8806_DIAG_HIST_PROXIMITY || hType == TMF8806_DIAG_HIST_ELECTRICAL_CAL )
		{
			tmf8806ReadSingleHistogram( driver, id + 4 );       // 4 packets are needed for a single histogram
			tmf8806ReadSingleHistogram( driver, id + 8 );       // 4 packets are needed for a single histogram
			tmf8806ReadSingleHistogram( driver, id + 0xC );     // 4 packets are needed for a single histogram
			tmf8806ReadSingleHistogram( driver, id + 0x10 );    // 4 packets are needed for a single histogram
		}
		else if ( hType != TMF8806_DIAG_HIST_ALG_PU_TDC_SUM && hType != TMF8806_DIAG_HIST_ALG_PILEUP )
		{
			return APP_ERROR_UNKNOWN_HISTOGRAM;      // was not a valid histogram type
		}
		// send continue command
		driver->dataBuffer[ 0 ] = TMF8806_APP_CMD_STAT__cmd_continue;
		i2cTxReg( driver, driver->i2cSlaveAddress, TMF8806_COM_CMD_REG, 1, driver->dataBuffer );
		res = tmf8806AppCheckCommandDone( driver, TMF8806_APP_CMD_STAT__cmd_continue, APP_CMD_CLEARED_TIMEOUT_MS );
	}
	return res;
}

// function sets the thresholds and persistence for interrupt trigger
int8_t tmf8806SetThresholds ( tmf8806Driver * driver, uint8_t persistence, uint16_t lowThreshold, uint16_t highThreshold )
{
	driver->dataBuffer[0] = persistence;
	tmf8806SetUint16( lowThreshold,	&(driver->dataBuffer[1]) );
	tmf8806SetUint16( highThreshold, &(driver->dataBuffer[3]) );
	driver->dataBuffer[5] = TMF8806_COM_CMD_STAT__cmd_wr_add_config;
	i2cTxReg( driver, driver->i2cSlaveAddress, TMF8806_COM_CMD_REG - 5, 6, driver->dataBuffer );
	return tmf8806AppCheckCommandDone( driver, TMF8806_COM_CMD_STAT__cmd_wr_add_config, APP_CMD_CLEARED_TIMEOUT_MS );
}

// function gets the thresholds and persistence for interrupt trigger
int8_t tmf8806GetThresholds ( tmf8806Driver * driver, uint8_t * persistence, uint16_t * lowThreshold, uint16_t * highThreshold )
{
	int8_t res;
	*persistence = 0;               // clear in case below communcation with device fails at some point
	*lowThreshold = 0;
	*highThreshold = 0;
	driver->dataBuffer[0] = TMF8806_COM_CMD_STAT__cmd_rd_add_config;
	i2cTxReg( driver, driver->i2cSlaveAddress, TMF8806_COM_CMD_REG, 1, driver->dataBuffer );
	res = tmf8806AppCheckCommandDone( driver, TMF8806_COM_CMD_STAT__cmd_rd_add_config, APP_CMD_CLEARED_TIMEOUT_MS );
	if ( res == APP_SUCCESS_OK )
	{
		i2cRxReg( driver, driver->i2cSlaveAddress, TMF8806_COM_CONTENT, 2+5, driver->dataBuffer ); // content, tid, 5 bytes for presistence, min-threshold, max-threshold
		if ( driver->dataBuffer[0] == TMF8806_COM_CMD_STAT__cmd_rd_add_config )
		{
			*persistence = driver->dataBuffer[2+0];
			*lowThreshold = tmf8806GetUint16( &( driver->dataBuffer[2+1] ) );
			*highThreshold = tmf8806GetUint16( &( driver->dataBuffer[2+3] ) );
		}
		else
		{
			res = APP_ERROR_NO_ADD_CFG;
		}
	}
	return res;
}

// function to set the slave address - address will become active as soon as the pattern matches the
// specified one: i.e. ( RReg( gpioData ) & gpioMask ) == ( gpioPattern & gpioMask )
// So for this function to succeed, the gpio pattern must be applied before calling this function
int8_t tmf8806SetI2CSlaveAddress ( tmf8806Driver * driver, uint8_t slaveAddress, uint8_t gpioPattern, uint8_t gpioMask )
{
	int8_t res;
	uint8_t prevAddr = driver->i2cSlaveAddress;
	driver->dataBuffer[0] = slaveAddress << 1;                              // 7-bit shifted
	driver->dataBuffer[1] = ( (gpioPattern & 3) << 2 ) | ( gpioMask & 3 ); // pattern + mask for gpio1,0
	driver->dataBuffer[2] = TMF8806_COM_CMD_STAT__cmd_wr_i2c_address;
	i2cTxReg( driver, driver->i2cSlaveAddress, TMF8806_COM_CMD_REG - 2, 3, driver->dataBuffer );
	driver->i2cSlaveAddress = slaveAddress;                                 // poll with new address
	res = tmf8806AppCheckCommandDone( driver, TMF8806_COM_CMD_STAT__cmd_wr_i2c_address, APP_CMD_CLEARED_TIMEOUT_MS );
	if ( res != APP_SUCCESS_OK )
	{
		driver->i2cSlaveAddress = prevAddr;         // failed to change, switch back to original i2c slave address
		res = APP_ERROR_I2C_ADDR_SET;
	}
	return res;
}

// ---------------------------------------------------------------------------------------------------------

// this function is CPU/compiler specific, bitfields and endianess are not generally defined
void tmf8806SerializeFactoryCalibration ( const tmf8806FactoryCalibData * factoryCalib, uint8_t * buffer )
{
	uint32_t a = factoryCalib->id;
	uint32_t b = factoryCalib->crosstalkIntensity;

	buffer[0] = (a & 0x0F) | ((b << 4) & 0xF0);
	buffer[1] = (b >>4) & 0xFF;
	buffer[2] = (b >> 12) & 0xFF;
	buffer[3] = factoryCalib->crosstalkTdc1Ch0BinPosUQ6Lsb;     // 8 bit

	a = factoryCalib->crosstalkTdc1Ch0BinPosUQ6Msb;             // 4 bit
	b = factoryCalib->crosstalkTdc1Ch1BinPosDeltaQ6;            // 9 bit
	buffer[4] = (a & 0x0F) | ((b << 4) & 0xF0);
	a = b >> 4;                                                 // 4 used
	b = factoryCalib->crosstalkTdc2Ch0BinPosDeltaQ6;            // 9 bit
	buffer[5] = (a & 0x1F) | ((b << 5) & 0xE0);
	a = b >> 3;                                                 // 3 used
	b = factoryCalib->crosstalkTdc2Ch1BinPosDeltaQ6;            // 9 bit
	buffer[6] = (a & 0x3F) | ((b << 6) & 0xC0);
	a = b >> 2;                                                 // 2 used
	b = factoryCalib->crosstalkTdc3Ch0BinPosDeltaQ6Lsb;         // 1 bit
	buffer[7] = (a & 0x7F) | ((b << 7) & 0x80);

	buffer[8] = factoryCalib->crosstalkTdc3Ch0BinPosDeltaQ6Msb; // 8 bit
	a = factoryCalib->crosstalkTdc3Ch1BinPosDeltaQ6;            // 9 bit
	buffer[9] = (a & 0xFF);
	a = a >> 8;                                                 // 8 used
	b = factoryCalib->crosstalkTdc4Ch0BinPosDeltaQ6;            // 9 bit
	buffer[10] = (a & 0x01) | ((b << 1) & 0xFE);
	a = b >> 7;                                                 // 7 used
	b = factoryCalib->crosstalkTdc4Ch1BinPosDeltaQ6Lsb;         // 6 bit
	buffer[11] = (a & 0x03) | ((b << 2) & 0xFC);

	a = factoryCalib->crosstalkTdc4Ch1BinPosDeltaQ6Msb;         // 3 bit
	b = factoryCalib->reserved;                                 // 5 bit
	buffer[12] = (a & 0x03 ) | ((b << 3) & 0xFC);
	buffer[13] = factoryCalib->opticalOffsetQ3;
}

// this function is CPU/compiler specific, bitfields and endianess are not generally defined
void tmf8806DeserializeFactoryCalibration ( const uint8_t * buffer, tmf8806FactoryCalibData * factoryCalib )
{
	factoryCalib->id = (buffer[0] & 0x0F);
	factoryCalib->crosstalkIntensity = (buffer[0] >> 4) | (((uint16_t)(buffer[1])) << 4 ) | (((uint32_t)(buffer[2])) << 12); // 20 bit
	factoryCalib->crosstalkTdc1Ch0BinPosUQ6Lsb = buffer[3];                                                 // 8 bit

	factoryCalib->crosstalkTdc1Ch0BinPosUQ6Msb = buffer[4] & 0x0F;                                          // 4 bit
	factoryCalib->crosstalkTdc1Ch1BinPosDeltaQ6 = (buffer[4] >> 4) | (((uint16_t)(buffer[ 5] & 0x1F)) << 4);// 9 bit
	factoryCalib->crosstalkTdc2Ch0BinPosDeltaQ6 = (buffer[5] >> 5) | (((uint16_t)(buffer[ 6] & 0x3F)) << 3);// 9 bit
	factoryCalib->crosstalkTdc2Ch1BinPosDeltaQ6 = (buffer[6] >> 6) | (((uint16_t)(buffer[ 7] & 0x7F)) << 2);// 9 bit
	factoryCalib->crosstalkTdc3Ch0BinPosDeltaQ6Lsb = buffer[7] >> 7;                                        // 1 bit

	factoryCalib->crosstalkTdc3Ch0BinPosDeltaQ6Msb = buffer[8];                                               // 8 bit
	factoryCalib->crosstalkTdc3Ch1BinPosDeltaQ6 = buffer[9] |         (((uint16_t)(buffer[10] & 0x01)) << 8); // 9 bit
	factoryCalib->crosstalkTdc4Ch0BinPosDeltaQ6 = (buffer[10] >> 1) | (((uint16_t)(buffer[11] & 0x03)) << 7); // 9 bit
	factoryCalib->crosstalkTdc4Ch1BinPosDeltaQ6Lsb = buffer[11] >> 2;                                         // 6 bit

	factoryCalib->crosstalkTdc4Ch1BinPosDeltaQ6Msb = buffer[12] & 0x07;                                   // 3 bit
	factoryCalib->reserved = buffer[12] >> 3;                                                             // 5 bit
	factoryCalib->opticalOffsetQ3 = buffer[13];                                                           // 8 bit
}

// this function is CPU/compiler specific, bitfields and endianess are not generally defined
void tmf8806SerializeStateData ( const tmf8806StateData * stateData, uint8_t * buffer )
{
	uint8_t a = stateData->id;                                  // 4 bit
	uint8_t b = stateData->reserved0;                           // 4 bit
	buffer[0] = (a & 0x0F) | (b << 4);
	buffer[1] = stateData->breakDownVoltage;                    // 8 bit
	tmf8806SetUint16( stateData->avgRefBinPosUQ9, &(buffer[2]) ); // 16-bit - UQ7.9

	buffer[4] = (uint8_t)(stateData->calTemp);
	buffer[5] = (uint8_t)(stateData->force20MhzVcselTemp);
	for ( int8_t i = 0; i < TMF8806_NUMBER_BIN_CALIB_VALUES; i++ )
	{
		buffer[6+i] = (uint8_t)(stateData->tdcBinCalibrationQ9[i]); // from signed to unsigned
	}
}

// this function is CPU/compiler specific, bitfields and endianess are not generally defined
void tmf8806DeserializeStateData ( const uint8_t * buffer, tmf8806StateData * stateData )
{
	stateData->id = buffer[0] & 0x0F;                                 // 4 bit
	stateData->reserved0 = (buffer[0] >> 4) & 0x0F;                   // 4 bit
	stateData->breakDownVoltage = buffer[1];                          // 8 bit
	stateData->avgRefBinPosUQ9 = tmf8806GetUint16( &(buffer[2]) );    // 16 bit - UQ7.9

	stateData->calTemp = (int8_t)(buffer[4]);
	stateData->force20MhzVcselTemp = (int8_t)(buffer[5]);
	for ( int8_t i = 0; i < TMF8806_NUMBER_BIN_CALIB_VALUES; i++ )
	{
		stateData->tdcBinCalibrationQ9[i] = (int8_t)(buffer[6+i]);
	}
}

// this function is CPU/compiler specific, bitfields and endianess are not generally defined
void tmf8806SerializeConfiguration ( const tmf8806MeasureCmd * config, uint8_t * buffer )
{
	buffer[0] =	config->data.spreadSpecSpadChp.amplitude
						| (config->data.spreadSpecSpadChp.config<< 4)
						| (config->data.spreadSpecSpadChp.reserved << 6);
	buffer[1] =	config->data.spreadSpecVcselChp.amplitude
						| (config->data.spreadSpecVcselChp.config<< 4)
						| (config->data.spreadSpecVcselChp.singleEdgeMode << 6)
						| (config->data.spreadSpecVcselChp.reserved<< 7);
	buffer[2] =	config->data.data.factoryCal
						| ( config->data.data.algState<< 1)
						| ( config->data.data.reserved<< 2)
						| ( config->data.data.spadDeadTime<< 3)
						| ( config->data.data.spadSelect<< 6);
	buffer[3] =	 config->data.algo.reserved0
						| ( config->data.algo.distanceEnabled<< 1 )
						| ( config->data.algo.vcselClkDiv2<< 2 )
						| ( config->data.algo.distanceMode<< 3 )
						| ( config->data.algo.immediateInterrupt<< 4 )
						| ( config->data.algo.reserved<< 5 )
						| ( config->data.algo.algKeepReady<< 7 );
	buffer[4] =	 config->data.gpio.gpio0
						| ( config->data.gpio.gpio1 << 4 );
	buffer[5] =	 config->data.daxDelay100us;
	buffer[6] =	 config->data.snr.threshold
						| ( config->data.snr.vcselClkSpreadSpecAmplitude << 6 );
	buffer[7] = config->data.repetitionPeriodMs;
	tmf8806SetUint16( config->data.kIters, &(buffer[8]) ); // 16-bit kilo-iterations
	buffer[10] = config->data.command;
}

// this function is CPU/compiler specific, bitfields and endianess are not generally defined
void tmf8806DeserializeConfiguration ( const uint8_t * buffer, tmf8806MeasureCmd * config )
{
	config->data.spreadSpecSpadChp.amplitude =  buffer[0] & 0xF;
	config->data.spreadSpecSpadChp.config    = (buffer[0] >> 4) & 3;
	config->data.spreadSpecSpadChp.reserved  = (buffer[0] >> 6) & 3;
	config->data.spreadSpecVcselChp.amplitude      =  buffer[1] & 0xF;
	config->data.spreadSpecVcselChp.config         = (buffer[1] >> 4) &   3;
	config->data.spreadSpecVcselChp.singleEdgeMode = (buffer[1] >> 6) &   1;
	config->data.spreadSpecVcselChp.reserved       = (buffer[1] >> 7) &   1;
	config->data.data.factoryCal          =  buffer[2]       & 1;
	config->data.data.algState            = (buffer[2] >> 1) & 1;
	config->data.data.reserved            = (buffer[2] >> 2) & 1;
	config->data.data.spadDeadTime        = (buffer[2] >> 3) & 7;
	config->data.data.spadSelect          = (buffer[2] >> 6) & 3;
	config->data.algo.reserved0           =  buffer[3]       & 1;
	config->data.algo.distanceEnabled     = (buffer[3] >> 1) & 1;
	config->data.algo.vcselClkDiv2        = (buffer[3] >> 2) & 1;
	config->data.algo.distanceMode        = (buffer[3] >> 3) & 1;
	config->data.algo.immediateInterrupt  = (buffer[3] >> 4) & 1;
	config->data.algo.reserved            = (buffer[3] >> 5) & 3;
	config->data.algo.algKeepReady        = (buffer[3] >> 7) & 1;
	config->data.gpio.gpio0               = buffer[4] & 0xF;
	config->data.gpio.gpio1               = (buffer[4] >> 4) & 0xF;
	config->data.daxDelay100us            = buffer[5];
	config->data.snr.threshold            = buffer[6] & 0x3F;
	config->data.snr.vcselClkSpreadSpecAmplitude = (buffer[6] >> 6) & 3;
	config->data.repetitionPeriodMs       = buffer[7];
	config->data.kIters                   = tmf8806GetUint16( &(buffer[8]) );
	config->data.command                  = buffer[10];
}


// this function is CPU/compiler specific, bitfields and endianess are not generally defined
// this function only exists for test purposes
void tmf8806SerializeResultFrame ( const tmf8806DistanceResultFrame * result, uint8_t * buffer )
{
	buffer[0] = result->resultNum;
	buffer[1] = result->reliability | (result->resultStatus << 6);
	tmf8806SetUint16( result->distPeak, &(buffer[2]) );
	tmf8806SetUint32( result->sysClock, &(buffer[4]) );
	for ( uint8_t i = 0; i < TMF8806_COM_STATE_DATA_COMPRESSED; i++)
	{
		buffer[8+i] = result->stateData[i];       // flat copy
	}
	buffer[8+TMF8806_COM_STATE_DATA_COMPRESSED] = result->temperature;
	tmf8806SetUint32( result->referenceHits, &(buffer[ 9+TMF8806_COM_STATE_DATA_COMPRESSED]) );
	tmf8806SetUint32( result->objectHits,    &(buffer[13+TMF8806_COM_STATE_DATA_COMPRESSED]) );
	tmf8806SetUint16( result->xtalk,         &(buffer[17+TMF8806_COM_STATE_DATA_COMPRESSED]) );
}

// this function is CPU/compiler specific, bitfields and endianess are not generally defined
void tmf8806DeserializeResultFrame ( const uint8_t * buffer, tmf8806DistanceResultFrame * result )
{
	result->resultNum = buffer[0];
	result->reliability = buffer[1] & 0x3F;
	result->resultStatus = buffer[1] >> 6;
	result->distPeak = tmf8806GetUint16( &( buffer[2] ) );
	result->sysClock = tmf8806GetUint32( &( buffer[4] ) );;
	for ( uint8_t i = 0; i < TMF8806_COM_STATE_DATA_COMPRESSED; i++)
	{
		result->stateData[i] = buffer[8+i];       // flat copy
	}
	result->temperature = buffer[8+TMF8806_COM_STATE_DATA_COMPRESSED];
	result->referenceHits = tmf8806GetUint32( &(buffer[ 9+TMF8806_COM_STATE_DATA_COMPRESSED]) );
	result->objectHits    = tmf8806GetUint32( &(buffer[13+TMF8806_COM_STATE_DATA_COMPRESSED]) );
	result->xtalk         = tmf8806GetUint16( &(buffer[17+TMF8806_COM_STATE_DATA_COMPRESSED]) );
}

