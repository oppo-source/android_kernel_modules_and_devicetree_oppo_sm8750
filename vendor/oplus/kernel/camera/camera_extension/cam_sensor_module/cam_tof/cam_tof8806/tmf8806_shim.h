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

#ifndef TMF8806_SHIM_EVM_H
#define TMF8806_SHIM_EVM_H
// ---------------------------------------------- includes ----------------------------------------
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/time.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/string.h>
#include <linux/unistd.h>

#include "tmf8806_regs.h"
#include "ams_i2c.h"
#include "tmf8806.h"
#include "cam_debug.h"

// ---------------------------------------------- defines -----------------------------------------
// on the ams FTDI evm the enable pin is connected to ACBUS bit 1, interupt to ACBUS bit 0
// below are the bit-masks
#define ENABLE_PIN                                2
#define INTERRUPT_PIN                             1

// i2c definitions

#define APP0_CMD_DATA_9             0x06

// ---------------------------------------------- macros ------------------------------------------

#define PROGMEM
/** @brief macros to cast a pointer to an address - adapt for your machine-word size
 */
#define PTR_TO_UINT(ptr)                     ( (uint64_t)(ptr) )

/** @brief macros to replace the platform specific printing
 */
#define PRINT_CHAR(c)                         pr_cont( "%c", c )
#define PRINT_INT(i)                          pr_cont( "%ld", (long)i )
#define PRINT_UINT(i)                         pr_cont( "%lu", (unsigned long)i )
#define PRINT_UINT_HEX(i)                     pr_cont( "%lX", (unsigned long)i )
#define PRINT_STR(str)                        pr_cont( "CAM_TOF %s", str )
#define PRINT_LN()                            pr_cont( "\n" )

/** Which character to use to seperate the entries in printing */
#define SEPARATOR                             ','

// for clock correction insert here the number in relation to your host
#define HOST_TICKS_PER_10_US                  10         /**< number of host ticks every 10 microseconds */
#define TMF8806_TICKS_PER_10_US               47         /**< number of tmf8806 ticks every 10 mircoseconds (4.7x faster than host) */

// ---------------------------------------------- forward declaration -----------------------------
// forward declaration of driver structure to avoid cyclic dependancies
typedef struct _tmf8806Driver tmf8806Driver;
// ---------------------------------------------- functions ---------------------------------------

int readPin( struct gpio_desc *gpiod );

int writePin( struct gpio_desc *gpiod, uint8_t value );

// ---------------------------------------------- functions ---------------------------------------

/** @brief Function to allow to wait for some time in microseconds
 *  @param[in] wait number of microseconds to wait before this function returns
 */
void delayInMicroseconds( uint32_t wait );

/** @brief Function returns the current sys-tick.
 * \return current system tick (granularity is host specific - see macro HOST_TICKS_PER_10_US)
 */
uint32_t getSysTick( void );

/** @brief Function reads a single byte from the given address. This is only needed on
 * systems that have special memory access methods for constant segments. Like e.g. Arduino Uno
 *  @param[in] address absolute memory address to read from
 * \return single byte from the given address
 */
uint8_t readProgramMemoryByte( uint64_t address );
/** @brief Function sets the enable pin HIGH. Note that the enable pin must be configured
 * for output (with e.g. function pinOutput)
 * @param[in] dptr ... a pointer to a data structure the function needs for setting the enable pin, can
 * be 0-pointer if the function does not need it
 */
int enablePinHigh( void * dptr );

/** @brief Function sets the enable pin LOW. Note that the enable pin must be configured
 * for output (with e.g. function pinOutput)
 * @param[in] dptr ... a pointer to a data structure the function needs for setting the enable pin, can
 * be 0-pointer if the function does not need it
 */
int enablePinLow( void * dptr );


/** @brief Function to read a 128 bytes == 1/4 of a 256 bin (2bytes) histogram
 * needed as reading register 0x30 will lead to register bank switching
 * on the device and the arduino uno can only read chunks of 32 bytes
 * So this function reads the quater histogram in reverse order.
 * @param[in] dptr ... a pointer to a data structure the function needs for transmitting, can
 * be 0-pointer if the function does not need it
 * @param[in] slaveAddr ... the i2c slave address to be used (7-bit)
 * @param[in] buffer ... a pointer to a buffer of minimum size of 128 bytes
 * \return 0 when successfully transmitted, else an error code
 */
int8_t tmf8806ReadQuadHistogram( void * dptr, uint8_t slaveAddr, uint8_t * buffer );

// ---------------------------------- I2C functions ---------------------------------------------

/**  Return codes for i2c functions:
 */
#define I2C_SUCCESS             0       /**< successfull execution no error */
#define I2C_ERROR              -1       /**< i2c error */

/** There are 2 styles of functions available:
 * 1. those that always require a register address to be specified: i2cTxReg, i2cRxReg
 * 2. the more generic (more I2C standard like) that can transmit and/or receive (here the
 *  register address if needed is the first transmitted byte): i2cTxRx
 * Only one set of those two *have to be* available. Both can be available.
 */

/** @brief I2C transmit only function.
 * @param[in] dptr a pointer to a data structure the function needs for transmitting, can
 * be 0-pointer if the function does not need it
 * @param[in] slaveAddr the i2c slave address to be used (7-bit unshifted)
 * @param[in] regAddr the register to start writing to
 * @param[in] toTx number of bytes in the buffer to transmit
 * @param[in] txData pointer to the buffer to transmit
 * \return 0 when successfully transmitted, else an error code
 */
int8_t i2cTxReg( void * dptr, uint8_t slaveAddr, uint8_t regAddr, uint16_t toTx, const uint8_t * txData );

/** @brief I2C transmit register address and receive function.
 * @param[in] dptr a pointer to a data structure the function needs for receiving, can
 * be 0-pointer if the function does not need it
 * @param slaveAddr the i2c slave address to be used (7-bit)
 * @param regAddr the register address to start reading from
 * @param toRx number of bytes in the buffer to receive
 * @param rxData pointer to the buffer to be filled with received bytes
 * \return 0 when successfully received, else an error code
 */
int8_t i2cRxReg( void * dptr, uint8_t slaveAddr, uint8_t regAddr, uint16_t toRx, uint8_t * rxData );

/** @brief I2C transmit and receive function.
 * @param dptr a pointer to a data structure the function needs for transmitting, can
 * be 0-pointer if the function does not need it
 * @param slaveAddr the i2c slave address to be used (7-bit)
 * @param toTx number of bytes in the buffer to transmit (set to 0 if receive only)
 * @param txData pointer to the buffer to transmit
 * @param toRx number of bytes in the buffer to receive (set to 0 if transmit only)
 * @param rxData pointer to the buffer to be filled with received bytes
 * \return 0 when successfully transmitted and received, else an error code
 */
int8_t i2cTxRx( void * dptr, uint8_t slaveAddr, uint16_t toTx, const uint8_t * txData, uint16_t toRx, uint8_t * rxData );

/** @brief Function to print a single TDC channel histogram in a kind of CSV like format
 * @param dptr a pointer to a data structure the function needs for transmitting, can
 * be 0-pointer if the function does not need it
 * @param histType type of histogram that will is in the buffer data
 * @param id the id for further identification of the histogram's TDC/channel
 * @param data a buffer of 256 bytes that represent the histogram in little endian encoding for 16-bit
 * @param scale a scaling factor. Each raw 16-bit value needs to be shifted by this amount to the left
 * This function is directly called by the driver to dump a histogram to the UART
 */
void tmf8806ScaleAndPrintHistogram( void * dptr, uint8_t histType, uint8_t id, uint8_t * data, uint8_t scale );

/** @brief Function waits for CPU ready
 * @param driver ... a pointer to the tmf8828 driver
 * @param waitInMs ... max wait time in ms
 * @return  0 when Cpu is not ready, 1 if Cpu is ready
 */
int8_t tmf8806IsApp0Ready( tmf8806Driver * driver, uint8_t waitInMs );

/** @brief Process the irq, readout of data
 * @param dptr ... a pointer to a data structure the function needs for transmitting
 * @return 0.. no data read, 1.. data read
 */
int tmf8806_app_process_irq( void * dptr );

/** @brief Oscillator trimming
 * @param driver ... a pointer to the tmf8828 driver
 * @param trim_value ... trim value address
 * @param write ... 1.. write to fuse, 2 .. read from fuse
 * @return  0 for no Error, else Error
 */
int tmf8806_oscillator_trim( tmf8806Driver * driver, int * trim_value, char write );

#endif /* TMF8806_SHIM_EVM_H */