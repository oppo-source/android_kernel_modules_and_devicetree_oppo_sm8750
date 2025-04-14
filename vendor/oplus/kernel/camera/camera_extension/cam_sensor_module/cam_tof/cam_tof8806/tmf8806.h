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

/** @file This is the tmf8806 driver.
 * This file contains all functionality to control the tmf8806. It uses the
 * shim for accessing the host HW itself.
 */

#ifndef TMF8806_H
#define TMF8806_H


// ---------------------------------------------- includes ----------------------------------------

#include "tmf8806_shim.h"
#include "tmf8806_regs.h"
#include "cam_debug.h"

#if defined( __cplusplus)
extern "C"
{
#endif

// ---------------------------------------------- defines -----------------------------------------

/** Driver version number
 * 1.0 ... initial working version for tmf8806 (tmf8805 patch)
 * 1.1 ... added support for interrupts
 * 1.2 ... added support for device and driver info records
 * 1.3 ... interrupt pin driven handling also supported, fixed bug in uint32_t decoding on Arduino
 *         Uno (implicit cast was only to uint16_t not uint32_t) by using explicit casting
 * 1.4 ... support for info records added
 * 1.5 ... support for thresholds, persistence and i2c address change
 * 1.6 ... now check also for application commands if the state-machine changes into
 *         state error (fast exit)
 * 1.7 ... error handling streamlined
 * 1.8 ... device sys-tick is only valid if LSB is set
 * 1.9 ... correct to 4.7 MHz oscillator, add configurable factory calibration
 * 1.10 .. increase wait time window for wake-up to 2 ms
 * 1.11 .. updated comments after review from M. Pelzmann
 * 1.12 .. added delay for wakeup sequence of 200 usec. to allow device to wakeup properly
 * 1.13 .. added a function to wait for cold-start shutdown of device
 * 2.1 ... adapted to use a true tmf8806
 * 2.2 ... refactored to be better portable
 * 2.3 ... more cleanup after review
 * 2.4 ... modification of shim layer for reading single bytes
 * 2.5 ... fixed clock ratio computation for clock correction
*/
#define TMF8806_DRIVER_MAJOR_VERSION  2
#define TMF8806_DRIVER_MINOR_VERSION  5

/** simple macro to construct a register bit-mask */
#define REG_MASK(F)  ( ((1 << (F##__WIDTH)) - 1) << (F##__SHIFT) )




/** how many histograms there are maximum */
#define TMF8806_NUMBER_HISTOGRAMS    5

/** histogram dumping selection bit-mask */
#define TMF8806_DUMP_HIST_ELECTRICAL_CAL     1
#define TMF8806_DUMP_HIST_PROXIMITY          2
#define TMF8806_DUMP_HIST_DISTANCE           4
#define TMF8806_DUMP_HIST_ALG_PILEUP         8
#define TMF8806_DUMP_HIST_ALG_PU_TDC_SUM     16


/** tmf8806 has as default i2c slave address 0x41 */
#define TMF8806_SLAVE_ADDR          0x41

/* important wait timings */
#define CAP_DISCHARGE_TIME_MS       3                     /**< wait time until we are sure the PCB's CAP has dischared properly */
#define ENABLE_TIME_MS              10                     /**< wait time after enable pin is high */
#define CPU_READY_TIME_MS           2                     /**< wait time for CPU ready */

/* return codes from the bootloader */
#define BL_SUCCESS_OK               0                     /**< bootloader success */
#define BL_ERROR_CMD                -1                    /**< bootloader command/communication failed */
#define BL_ERROR_TIMEOUT            -2                    /**< bootloader communication timeout */

/* return codes from the measurement application */
#define APP_SUCCESS_OK              (BL_SUCCESS_OK)       /**< application success */
#define APP_ERROR_CMD               (BL_ERROR_CMD)        /**< application command/communication failed */
#define APP_ERROR_TIMEOUT           (BL_ERROR_TIMEOUT)    /**< application timeout */
#define APP_ERROR_PARAM                     -3            /**< application invalid parameter (e.g. spad map id wrong) */
#define APP_ERROR_NO_RESULT_PAGE            -4            /**< application did not receive a measurement result page */
#define APP_ERROR_NO_FACTORY_CALIB_PAGE     -5            /**< application did not receive a factory calib page */
#define APP_ERROR_UNKNOWN_HISTOGRAM         -6            /**< application histogram type is not supported */
#define APP_ERROR_NO_SERIAL_NUMBER          -7            /**< application serial number read failed (or app is not running) */
#define APP_ERROR_NO_ADD_CFG                -8            /**< application could not read additional configuration page */
#define APP_ERROR_I2C_ADDR_SET              -9            /**< application switching of i2c address failed */

/* Interrupt bits */
#define TMF8806_INTERRUPT_RESULT          0x01            /**< bit 0 == result interrupt */
#define TMF8806_INTERRUPT_DIAGNOSTIC      0x02            /**< bit 1 == diagnostic interrupt */

/* some important register addresses */
#define TMF8806_COM_CMD_REG               0x10            /**< command register address */
#define TMF8806_COM_CMD_PREV              0x11            /**< previous command register address */
#define TMF8806_COM_CMD_DATA              (TMF8806_COM_CMD_REG - sizeof(tmf8806MeasureCmd) + 1)     /**< command parameter data starts in this register */

#define TMF8806_COM_MINOR_VERSION         0x12            /**< minor version register address */
#define TMF8806_COM_PATCH_VERSION         0x13            /**< patch version register address */
#define TMF8806_COM_DIAG_INFO             0x1a            /**< diagnostic register address */

/* result page addresses and defines */
#define TMF8806_COM_STATE                 0x1c            /**< application state register address */
#define TMF8806_COM_STATUS                0x1d            /**< application status register address */

#define TMF8806_COM_CONTENT               0x1e            /**< application content register defines what is in register 0x20 and following */
#define TMF8806_COM_TID                   0x1f            /**< application transaction id register address */

/* some more info registers from the results */
#define TMF8806_COM_RESULT_NUMBER         0x20            /**< result number register address */
#define TMF8806_COM_RESULT_CONFIDENCE     0x21
#define TMF8806_COM_DISTANCE_LSB          0x22
#define TMF8806_COM_DISTANCE_MSB          0x23
#define TMF8806_COM_SYS_TICK_0            0x24            /**< sys tick is 4 bytes in little endian */
#define TMF8806_COM_STATE_DATA0           0x28            /**< state data is 11 bytes */
#define TMF8806_COM_TEMPERATURE           0x33
#define TMF8806_COM_REF_HITS_0            0x34            /**< reference hits is 4 bytes in little endian */
#define TMF8806_COM_OBJ_HITS_0            0x38            /**< object hits is 4 bytes in little endian */
#define TMF8806_COM_XTALK_LSB             0x3C
#define TMF8806_COM_XTALK_MSB             0x3D            /**< last of result frame */

/** Use this macro like this: data[ RESULT_REG( RESULT_NUMBER ) ], it calculates the offset into the data buffer */
#define RESULT_REG( regName )                               ( (TMF8806_COM_##regName) - (TMF8806_COM_STATE) )

#define TMF8806_COM_CONFIG                                  0x20    /**< config register address, factory calibartion or state data is loaded here */
#define TMF8806_COM_STATE_DATA_COMPRESSED                   11      /**< size of state data */
#define TMF8806_COM_FACTORY_CALIB_DATA_COMPRESSED           13      /**< size of factory calibration data */

#define TMF8806_COM_RESULT__measurement_result              0x55 /**< measurement result == content register value */
#define TMF8806_COM_RESULT__measurement_result_size         (TMF8806_COM_XTALK_MSB - TMF8806_COM_STATE + 1)  /**< result size + leading bytes starting from state */

/* application commands */
#define TMF8806_COM_CMD_STAT__cmd_measure                   0x02  /**< application command to Start a measurement */
#define TMF8806_COM_CMD_STAT__cmd_wr_add_config             0x08  /**< application command to write additional configuration parameter */
#define TMF8806_COM_CMD_STAT__cmd_rd_add_config             0x09  /**< application command to read additional configuration parameter */
#define TMF8806_COM_CMD_STAT__cmd_factory_calibration       0x0a  /**< application command to Perform a factory calibration */
#define TMF8806_COM_CMD_STAT__cmd_read_serial_number        0x47  /**< application command to read serial number */
#define TMF8806_COM_CMD_STAT__cmd_wr_i2c_address            0x49  /**< application command to change the i2c slave address with next pattern on gpio0/1 as specified */
#define TMF8806_COM_CMD_STAT__cmd_stop                      0xff  /**< application command Stop a measurement */

/* Clock correction pairs must be a power of 2 value. */
#define TMF8806_CLK_CORRECTION_PAIRS                        4     /**< how many clock correction pairs are stored, must be a power of 2 */

/** number of bins per tdc channel . A histogram has 128 bins. */
#define TMF8806_HISTOGRAM_BINS                              128

/** Buffer must be big enough to read a complete result page and the bootloader max transfer size into driver->dataBuffer */
#define TMF8806_DATA_BUFFER_SIZE                            (2*TMF8806_HISTOGRAM_BINS)


/* diagnostic header information for histograms */
#define TMF8806_DIAG_HIST_ELECTRICAL_CAL                    1     /**< bit-mask for electrical calibration histograms */
#define TMF8806_DIAG_HIST_PROXIMITY                         4     /**< bit-mask for proximity histograms */
#define TMF8806_DIAG_HIST_DISTANCE                          7     /**< bit-mask for distance histograms */
#define TMF8806_DIAG_HIST_ALG_PILEUP                        16    /**< bit-mask for pile-up corrected histograms */
#define TMF8806_DIAG_HIST_ALG_PU_TDC_SUM                    17    /**< bit-mask for pile-up corrected sum histograms */


// ---------------------------------------------- logging -----------------------------------------

#define TMF8806_LOG_LEVEL_NONE                              0     /**< no logging not even errors */
#define TMF8806_LOG_LEVEL_ERROR                             1     /**< only error logging */
#define TMF8806_LOG_LEVEL_CLK_CORRECTION                    8     /**< this is a bit-mask check for clock correction logging */
#define TMF8806_LOG_LEVEL_INFO                              0x10  /**< some information */
#define TMF8806_LOG_LEVEL_VERBOSE                           0x20  /**< very chatty firmware */
#define TMF8806_LOG_LEVEL_I2C                               0x80  /**< this is a bit-mask check for i2c logging */
#define TMF8806_LOG_LEVEL_DEBUG                             0xFF  /**< dump everything */


// ---------------------------------------------- types -------------------------------------------

/** @brief driver info: compile time info of this driver
 */
typedef struct _tmf8806DriverInfo
{
	uint8_t version[2];                               /**< this driver version number major.minor*/
} tmf8806DriverInfo;

/** @brief device information: read from the device
 */
typedef struct _tmf8806DeviceInfo
{
	uint32_t deviceSerialNumber;                      /**< serial number of device, if 0 not read */
	uint8_t appVersion[4];                            /**< application version number (app id, major, minor, patch) */
	uint8_t chipVersion[2];                           /**< chip version (Id, revId) */
} tmf8806DeviceInfo;

/** @brief Each tmf8828 driver instance needs a data structure like this
 */
typedef struct _tmf8806Driver
{
	tmf8806DeviceInfo device;                         /**< information record of device */
	tmf8806DriverInfo info;                           /**< information record of driver */
	tmf8806MeasureCmd measureConfig;                  /**< current configuration */
	tmf8806FactoryCalibData factoryCalib;             /**< current factory calibration data */
	tmf8806StateData stateData;                       /**< current state data */
	uint32_t hostTicks[ TMF8806_CLK_CORRECTION_PAIRS ];    /**< host ticks for clock correction */
	uint32_t tmf8806Ticks[ TMF8806_CLK_CORRECTION_PAIRS ]; /**< device ticks for clock correction */
	uint16_t clkCorrRatioUQ;                          /**< UQ16 clock ratio [0..2) */
	uint8_t clkCorrectionIdx;                         /**< index of the last inserted pair */
	uint8_t clkCorrectionEnable;                      /**< default is clock correction on */
	uint8_t i2cSlaveAddress;                          /**< i2c slave address (7-bit unshifed) to talk to device */
	uint8_t logLevel;                                 /**< how chatty the program is */
	uint8_t dataBuffer[ TMF8806_DATA_BUFFER_SIZE ];   /**< driver scratch buffer for i2c tx/rx */
} tmf8806Driver;

/** declaration of default config to be accessible from outside */
extern const tmf8806MeasureCmd defaultConfig;

// ---------------------------------------------- functions ---------------------------------------
// Power and bootloader functions are available with ROM code.
// ---------------------------------------------- functions ---------------------------------------

/** @brief Function to initialise the driver data structure, call this as the first function
 * of your program, before using any other function of this driver
 * @param driver ... pointer to an instance of the tmf8806 driver data structure
 * @param logLevel ... the logging level of the driver until changed by fn tmf8806SetLogLevel
 */
void tmf8806Initialise( tmf8806Driver * driver, uint8_t logLevel );

/** @brief Function to configure the driver how chatty it should be. This is only the driver itself.
 * @param driver ... pointer to an instance of the tmf8806 driver data structure
 * @param level ... the log level for printing
 */
void tmf8806SetLogLevel( tmf8806Driver * driver, uint8_t level );

/** @brief Function to set clock correction on or off.
 * @param driver ... pointer to an instance of the tmf8806 driver data structure
 * @param enable ... if <>0 clock correction is enabled (default)
 * @param enable ... if ==0 clock correction is disabled
 */
void tmf8806ClkCorrection( tmf8806Driver * driver, uint8_t enable );

/** @brief Function to reset the HW and SW on the device
 * @param driver ... pointer to an instance of the tmf8806 driver data structure
 */
void tmf8806Reset( tmf8806Driver * driver );

/** @brief Function to set the enable pin high
 * @param driver ... pointer to an instance of the tmf8806 driver data structure
 */
void tmf8806Enable( tmf8806Driver * driver );

/** @brief Function to set the enable pin low
 * @param driver ... pointer to an instance of the tmf8806 driver data structure
 */
void tmf8806Disable( tmf8806Driver * driver );

/** @brief Function to put the device in standby mode
 * @param driver ... pointer to an instance of the tmf8806 driver data structure
 */
void tmf8806Standby( tmf8806Driver * driver );

/** @brief Function to wake the device up from standby mode
 * @param driver ... pointer to an instance of the tmf8806 driver data structure
 */
void tmf8806Wakeup( tmf8806Driver * driver );

/** @brief Function returns true if CPU is ready, else false. If CPU is not ready, device cannot be used.
 * @param driver ... pointer to an instance of the tmf8806 driver data structure
 * @return !=0 if CPU is ready (device can be used), else it returns ==0 (device cannot be accessed).
 */
int8_t tmf8806IsCpuReady( tmf8806Driver * driver, uint8_t waitInMs);

/** @brief Function to download the firmware image that was linked against the firmware (tmf8828_image.{h,c} files)
 * @param driver ... pointer to an instance of the tmf8806 driver data structure
 * @param imageStartAddress ... destination in RAM to which the image shall be downloaded
 * @param image ... pointer to the character array that represents the downloadable image
 * @param imageSizeInBytes ... the number of bytes to be downloaded
 * @param Function returns BL_SUCCESS_OK if successfully downloaded the FW, else it returns an error BL_ERROR_*
 */
int8_t tmf8806DownloadFirmware( tmf8806Driver * driver, uint32_t imageStartAddress, const uint8_t * image, int32_t imageSizeInBytes );

/** @brief Function to switch to the ROM application
 * @param driver ... pointer to an instance of the tmf8806 driver data structure
 * @param Function returns BL_SUCCESS_OK if successfully started the ROM application, else it returns an error BL_ERROR_*
 */
int8_t tmf8806SwitchToRomApplication( tmf8806Driver * driver );

// convenience functions ------------------------------------------------------------------------

/** @brief Convert 4 bytes in little endian format into an uint32_t
 * @param data ... pointer to the 4 bytes to be converted
 * @param returns the 32-bit value
 */
uint32_t tmf8806GetUint32( const uint8_t * data );

/** @brief Convert uint32_t in little endian format to 4 bytes
 * @param value ... the 32-bit value to be converted
 * @param data ... pointer to destinationw here the converted value shall be stored
 */
void tmf8806SetUint32( uint32_t value, uint8_t * data );

/** @brief Convert 2 bytes in little endian format into an uint16_t
 * @param data ... pointer to the 2 bytes to be converted
 * @return the converted 16-bit value
 */
uint16_t tmf8806GetUint16( const uint8_t * data );

/** @brief Convert uint16_t in little endian format to 2 bytes
 * @param value ... the 16-bit value to be converted
 * @param data ... pointer to destinationw here the converted value shall be stored
 */
void tmf8806SetUint16( uint16_t value, uint8_t * data );


// ------------------------------- application functions --------------------------------------------
// Application functions are only available after a successfull firmware download
// ------------------------------- application functions --------------------------------------------

/** @brief Function reads complete device information
 * @param driver ... pointer to an instance of the tmf8806 driver data structure
 * @return Function returns APP_SUCCESS_OK if successfully read the complete device information, else
 * it returns an error APP_ERROR_*
 */
int8_t tmf8806ReadDeviceInfo( tmf8806Driver * driver );

/** @brief Function to execute a factory calibration. Factory calibration will be done
 * with the configuration parameters that have been given to the driver (specifically adjust
 * the number of iterations if you want to do factory calibration with e.g. 4_000_000 iterations).
 * The caller needs to wait until a result interrupt is asserted. Then the function
 * tmf8806ReadFactoryCalibration shall be called to retrieve the factory calibration result.
 * @param driver ... pointer to an instance of the tmf8806 driver data structure
 * @param kIters ... the kilo-iterations to be used for factory calibration.
 * @return Function returns APP_SUCCESS_OK if successfully started the factory calibration, else APP_ERROR_*
 */
int8_t tmf8806FactoryCalibration( tmf8806Driver * driver, uint16_t kIters );

/** @brief Function to read factory calibration results. This function should only be called when there was a
 * result interrupt (use function tmf8806GetAndClrInterrupts to find this out).
 * @param driver ... pointer to an instance of the tmf8806 driver data structure
 * @return Function returns APP_SUCCESS_OK if there was a factory calibration successfully performed before
 * calling this function, else APP_ERROR_NO_FACTORY_CALIB_PAGE.
 */
int8_t tmf8806ReadFactoryCalibration( tmf8806Driver * driver );

/** @brief Function does store the given factory calibration data in the driver (will be used with next start command)
 * @param driver ... pointer to an instance of the tmf8806 driver data structure
 * @param calibData ... pointer to a factory calibration
 */
void tmf8806SetFactoryCalibration( tmf8806Driver * driver, const tmf8806FactoryCalibData * calibData );

/** @brief Function does store the given state data  in the driver (will be used with next factory calibration
 * or start command)
 * @param driver ... pointer to an instance of the tmf8806 driver data structure
 * @param stateData ... pointer to a complete state data
 */
void tmf8806SetStateData( tmf8806Driver * driver, const tmf8806StateData * stateData );

/** @brief Function does store the given configuration data in the driver (will be used with next
 * factory calibration command or start command)
 * @param driver ... pointer to an instance of the tmf8806 driver data structure
 * @param config ... pointer to a complete configuration
 */
void tmf8806SetConfiguration( tmf8806Driver * driver, const tmf8806MeasureCmd * config );

/** @brief Function to start a measurement (the configuration, factory calibration and state data are
 * used from the driver)
 * @param driver ... pointer to an instance of the tmf8806 driver data structure
 * @param kIter ... the kilo-iterations to be used for measurement
 * @return Function returns APP_SUCCESS_OK if successfully started a measurement, else it returns an error APP_ERROR_*
 */
int8_t tmf8806StartMeasurement( tmf8806Driver * driver );

/** @brief Function to stop a measurement
 * @param driver ... pointer to an instance of the tmf8806 driver data structure
 * @return Function returns APP_SUCCESS_OK if successfully stopped the application, else it returns an error APP_ERROR_*
 */
int8_t tmf8806StopMeasurement( tmf8806Driver * driver );

/** @brief Function reads the interrupts that are set and clears those.
 * @param driver ... pointer to an instance of the tmf8806 driver data structure
 * @return Function returns the bit-mask of the interrupts that were set.
 */
uint8_t tmf8806GetAndClrInterrupts( tmf8806Driver * driver, uint8_t mask );

/** @brief Function clears the given interrupts and enables them.
 * @param driver ... pointer to an instance of the tmf8806 driver data structure
 */
void tmf8806ClrAndEnableInterrupts( tmf8806Driver * driver, uint8_t mask );

/** @brief Function disable the given interrupts.
 * @param driver ... pointer to an instance of the tmf8806 driver data structure
 */
void tmf8806DisableAndClrInterrupts( tmf8806Driver * driver, uint8_t mask );

/** @brief Function to read results. This function should only be called when there was a
 * result interrupt (use function tmf8806GetAndClrInterrupts to find this out).
 * @param driver ... pointer to an instance of the tmf8806 driver data structure
 * @return Function returns APP_SUCCESS_OK if there was a result page, else APP_ERROR_NO_RESULT_PAGE.
 */
int8_t tmf8806ReadResult( tmf8806Driver * driver, tmf8806DistanceResultFrame * result );

/** @brief Correct the distance based on the clock correction pairs
 * @param driver ... pointer to an instance of the tmf8806 driver data structure
 */
uint16_t tmf8806CorrectDistance( tmf8806Driver * driver, uint16_t distance );

/** @brief Configure device to report histograms (non-real-time, blocking communication)
 * @param driver ... pointer to an instance of the tmf8806 driver data structure
 * @param histograms ... bitmask of enabled histgrams
 * @return Function returns APP_SUCCESS_OK if there was a result page, else it returns an error APP_ERROR_*
 */
int8_t tmf8806ConfigureHistograms( tmf8806Driver * driver, uint8_t histograms );

/** @brief Device reads one or more histograms (non-real-time, blocking communication, and reports
 * them with function )
 * @param driver ... pointer to an instance of the tmf8806 driver data structure
 * @return Function returns APP_SUCCESS_OK if there was a histogram, else it returns an error APP_ERROR_*
 */
int8_t tmf8806ReadHistograms( tmf8806Driver * driver );

/** @brief Function to set the interrupt trigger thresholds in mm
 * @param driver ... pointer to an instance of hte tmf8806 driver data structure
 * @param persistence ... if set to 0 every (even a 0-result) will trigger an interrupt,
 *                 if set to 1 only a non-zero result will trigger an interrupt
 *                 if set to n > 1, only if n consecutive results are non-zero will trigger an interrupt
 * @param lowThreshold ... if persistence is >0 a result distance in mm must be >= this value (in mm) to count as valid
 * @param highThreshold ... if persistence is >0 a result distance in mm must be <= this value (in mm) to count as valid
 * @return Function returns APP_SUCCESS_OK if the command was executed successully, else it returns an error APP_ERROR_*
 */
int8_t tmf8806SetThresholds( tmf8806Driver * driver, uint8_t persistence, uint16_t lowThreshold, uint16_t highThreshold );

/** @brief Function to read the interrupt trigger thresholds in mm
 * @param driver ... pointer to an instance of hte tmf8806 driver data structure
 * @param persistence ... pointer to an uint8_t variable will contain the persistence value
 * @param lowThreshold ... pointer to an uint16_t variable will contain the low threshold value
 * @param highThreshold ... pointer to an uint16_t variable will contain the high threshold value
 * @return Function returns APP_SUCCESS_OK if the command was executed successully, else it returns an error APP_ERROR_*
 */
int8_t tmf8806GetThresholds( tmf8806Driver * driver, uint8_t * persistence, uint16_t * lowThreshold, uint16_t * highThreshold );

/** @brief Function changes the 7-bit (unshifted) device slave address to the specified one
 * Note that the gpio pattern should be applied before calling this function. The reason is that the function
 * immediately changes the slaveAddress to the new one. If the pattern is not available the device will not
 * switch to the new i2c address and use the original one.
 * function will switch i2c address if the following is true: ((gpio & gpioMask) == (gpioPattern & gpioMask))
 * @param driver ... pointer to an instance of hte tmf8806 driver data structure
 * @param slaveAddress ... the unshifted 7-bit device i2c slave address
 * @param gpioPattern ... pattern that shall be used for deciding if address shall be changed (2-bits only)
 * @param gpioMask ... mask that shall be used for deciding if address shall be changed (2-bits only)
 * @return function returns APP_SUCCESS_OK if the command was executed successully, else it returns an error APP_ERROR_*
 */
int8_t tmf8806SetI2CSlaveAddress( tmf8806Driver * driver, uint8_t slaveAddress, uint8_t gpioPattern, uint8_t gpioMask );


// ------ serialize/deserialize functions --------------------------------------------------------

/** @brief Function converts CPU/compiler dependant a bitfield multi-byte structure
 * in the required byte stream
 * @param factoryCalib ... pointer to the factory calibration to be serialized
 * @param buffer ... pointer to the destination buffer (must be sizeof(tmf8806FactoryCalibData))
 */
void tmf8806SerializeFactoryCalibration( const tmf8806FactoryCalibData * factoryCalib, uint8_t * buffer );

/** @brief Function converts a byte-stream into a CPU/compiler dependant bitfield multi-byte structure
 * @param buffer ... pointer to the source buffer (must be sizeof(tmf8806FactoryCalibData))
 * @param factoryCalib ... pointer to the factory calibration to be filled with deserialized data stream
 */
void tmf8806DeserializeFactoryCalibration( const uint8_t * buffer, tmf8806FactoryCalibData * factoryCalib );

/** @brief Function converts CPU/compiler dependant a bitfield multi-byte structure
 * in the required byte stream
 * @param stateData ... pointer to the state data to be serialized
 * @param buffer ... pointer to the destination buffer (must be sizeof(tmf8806StateData))
 */
void tmf8806SerializeStateData( const tmf8806StateData * stateData, uint8_t * buffer );

/** @brief Function converts a byte-stream into a CPU/compiler dependant bitfield multi-byte structure
 * @param buffer ... pointer to the source buffer (must be sizeof(tmf8806StateData))
 * @param stateData ... pointer to the stateData to be filled with deserialized data stream
 */
void tmf8806DeserializeStateData( const uint8_t * buffer, tmf8806StateData * stateData );

/** @brief Function converts CPU/compiler dependant a bitfield multi-byte structure
 * in the required byte stream
 * @param config ... pointer to the measurement configuration to be serialized
 * @param buffer ... pointer to the destination buffer (must be sizeof(tmf8806MeasureCmd))
 */
void tmf8806SerializeConfiguration( const tmf8806MeasureCmd * config, uint8_t * buffer );

/** @brief Function converts a byte-stream into a CPU/compiler dependant bitfield multi-byte structure
 * @param buffer ... pointer to the source buffer (must be sizeof(tmf8806MeasureCmd))
 * @param stateData ... pointer to the configuration to be filled with deserialized data stream
 */
void tmf8806DeserializeConfiguration( const uint8_t * buffer, tmf8806MeasureCmd * config );

/** @brief Function converts CPU/compiler dependant a bitfield multi-byte structure
 * in the required byte stream
 * @param config ... pointer to the result frame to be serialized
 * @param buffer ... pointer to the destination buffer (must be sizeof(tmf8806DistanceResultFrame))
 */
void tmf8806SerializeResultFrame( const tmf8806DistanceResultFrame * result, uint8_t * buffer );

/** @brief Function converts a byte-stream into a CPU/compiler dependant bitfield multi-byte structure
 * @param buffer ... pointer to the source buffer (must be sizeof(tmf8806DistanceResultFrame))
 * @param stateData ... pointer to the result frame to be filled with deserialized data stream
 */
void tmf8806DeserializeResultFrame( const uint8_t * buffer, tmf8806DistanceResultFrame * result );

#if defined( __cplusplus)
}
#endif

#endif // TMF8806_H
