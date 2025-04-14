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

#ifndef TMF8806_REGS_H
#define TMF8806_REGS_H

/* APP_MINOR_VERSION / APP_VERSION_PATCH history
 * 0x00 / 0x00 ... initial version
 */

#define TMF880X_APP_VERSION_MAJOR   4
#define TMF880X_APP_VERSION_MINOR   1
#define TMF880X_APP_VERSION_PATCH   0

/* only use if header file is not included in an assembler file */
#if !defined( INCLUDE_IN_ASM ) || ( INCLUDE_IN_ASM == 0 )

#include "tmf8806_shim.h"

#define TMF8806_TEMP_INVALID 127 /*!< The temperature is not set, or out of range. */

/*! A Distance result frame. */
struct __attribute__((packed)) _tmf8806DistanceResultFrame
{
	uint8_t resultNum; /*!< Result number, incremented every time there is a unique answer */
	uint8_t reliability:6; /*!< Reliability of object measurement */
	uint8_t resultStatus:2; /*!< algEnhancedResult == 1: Will indicate the status of the measurement. algEnhancedResult == 0: Will indicate the status of the GPIO interrupt. */
	uint16_t distPeak; /*!< Distance to the peak of the object */
	uint32_t sysClock; /*!< System clock/time stamp in units of 0.2us. */
	uint8_t stateData[ 11 ]; /*!< Packed state data. Host can store this during a sleep, and re-upload it for the next measurement. */
	int8_t  temperature; /*!< The measurement temperature in degree celsius. */
	uint32_t referenceHits;  /*!< Sum of the reference SPADs during the distance measurement. WARNING: Unaligned offset. */
	uint32_t objectHits;  /*!< Sum of the object SPADs during the distance measurement. WARNING: Unaligned offset. */
	uint16_t xtalk; /*!< The crosstalk peak value. */

};
typedef struct _tmf8806DistanceResultFrame tmf8806DistanceResultFrame;

/*! The measure config and command. */
union _tmf8806MeasureCmd
{
	struct __attribute__((packed)) _tmf8806MeasureCmdRaw
	{
		uint8_t cmdData9; /*!< **NEW** Command Data 9. */
		uint8_t cmdData8; /*!< **NEW** Command Data 8. */
		uint8_t cmdData7; /*!< Command Data 7. */
		uint8_t cmdData6; /*!< Command Data 6. */
		uint8_t cmdData5; /*!< Command Data 5. */
		uint8_t cmdData4; /*!< Command Data 4. */
		uint8_t cmdData3; /*!< Command Data 3. */
		uint8_t cmdData2; /*!< Command Data 2. */
		uint8_t cmdData1; /*!< Command Data 1. */
		uint8_t cmdData0; /*!< Command Data 0. */
		uint8_t command;  /*!< The command code. */
	} packed;
	struct __attribute__((packed)) _tmf8806MeasureCmdConfig
	{
		 /*! **NEW** Spread spectrum (jitter) of the High Voltage (SPAD) charge pump clock */
		struct _tmf8806MeasureCmdSpreadSpectrumSpadChargePump
		{
			uint8_t amplitude:4;    /*!< Amplitude of spread spectrum mode of SPAD high voltage charge pump. 0=disabled, 1..15=amplitude of the jitter (100ps per step). */
			uint8_t config:2;         /*!< Configuration of spread spectrum mode of SPAD high voltage charge pump. 0=two-frequency mode, 1=fully random mode, 2=random walk mode, 3=re-use VCSEL charge pump clock (divided by 2). */
			uint8_t reserved:2;
		} spreadSpecSpadChp;

		 /*! **NEW** Spread spectrum (jitter) of the VCSEL charge pump clock */
		struct _tmf8806MeasureCmdSpreadSpectrumVcselChargePump
		{
			uint8_t amplitude:4;    /*!< Amplitude of spread spectrum mode of VCSEL high voltage charge pump. 0=disabled, 1..15=amplitude of the jitter (100ps per step). */
			uint8_t config:2;         /*!< Configuration of spread spectrum mode of VCSEL high voltage charge pump. 0=two-frequency mode, 1=fully random mode, 2=random walk mode, 3=reserved. */
			uint8_t singleEdgeMode:1; /*!< If set only randomize the VCSEL charge-pump clock at positive clock edge. */
			uint8_t reserved:1;
		} spreadSpecVcselChp;

		struct _tmf8806MeasureCmdDataSettings
		{
			uint8_t factoryCal:1;   /*!< When 1 the data includes factory calibration. */
			uint8_t algState:1;     /*!< When 1 the data includes algorithm state */
			uint8_t reserved:1;     /*!< **NEW** Deprecated feature. This value must be 0. */
			uint8_t spadDeadTime:3; /*!< **NEW** The SPAD dead time setting. 0=longest, 7=shortest dead time. */
			uint8_t spadSelect:2;   /*!< **NEW** Which SPADs to use (less SPADs are better for high crosstalk peak, but worse for SNR). 0=all, 1=40best, 2=20best, 3=attenuated. */
		} data;
		struct _tmf8806MeasureCmdAlgoSettings
		{
			uint8_t reserved0:1;
			uint8_t distanceEnabled:1;    /*!< When 1 distance+prox algorithm are executed (short + long range). When 0, only prox algorithm is executed (short range only). */
			uint8_t vcselClkDiv2:1;       /*!< When 1 operates the VCSEL clock at half frequency. 0=37.6MHz, 1=18.8MHz. */
			uint8_t distanceMode:1;       /*!< **NEW** When 0 measure up to 2.5m. When 1 measure up to 4m. 4m mode is only activated if the VCSEL clock is configured for 20MHz. Fall back to 2.5m mode if VCSEL clock is 40 MHz. */
			uint8_t immediateInterrupt:1; /*!< When 1 target distance measurement will immediately report to the host an interrupt of the capturing caused by a GPIO event.
											When 0, will only report to the host when target distance measurement was finished. */
			uint8_t reserved:2;           /*!< Was legacy result structure selector. Ignored. */
			uint8_t algKeepReady:1;       /*!< When 1 do not go to standby between measurements, and keep charge pump and histogram RAM between measurements */
		} algo;
		struct _tmf8806MeasureCmdGpioSettings
		{
			uint8_t gpio0:4; /*!< GPIO0 settings: 0=disable, 1=input low halts measurement, 2=input high halts measurement, 3=output DAX sync, 4= low, 5=high */
			uint8_t gpio1:4; /*!< GPIO1 settings: 0=disable, 1=input low halts measurement, 2=input high halts measurement, 3=output DAX sync, 4= low, 5=high */
		} gpio;
		uint8_t daxDelay100us;      /*!< DAX delay, 0 for no delay/signal, otherwise in units of 100uS */

		struct _tmf8806MeasureCmdSnrVcselSpreadSpecSettings
		{
			uint8_t threshold:6; /*!< The peak histogram signal-to-noise ratio. If set to 0, use default value (6). */
			uint8_t vcselClkSpreadSpecAmplitude:2; /*!< The VCSEL clock spread spectrum. 0=off, 1..3=Clock jitter settings. Only works if vcselClkDiv2=1. */
		} snr;
		uint8_t repetitionPeriodMs; /*!< Measurement period in ms, use 0 for single shot. NOTE: valid values are 0, 5 - 201 */
		uint16_t kIters;            /*!< Integration iteration *1000. If this value is 0xffff, the default number of iterations (1600*1000) is used. */
		uint8_t command;            /*!< The command code. */
    } data;
};
typedef union _tmf8806MeasureCmd tmf8806MeasureCmd;

/*! The new state data. */
struct __attribute__((packed)) _tmf8806StateData
{
	uint8_t  id:4; /*!< The state data ID. Must be 0x2, or the state data will be discarded. */
	uint8_t  reserved0: 4;
	uint8_t  breakDownVoltage; /*!< The last selected breakdown voltage value. */
	uint16_t avgRefBinPosUQ9; /*!< The average optical reference bin position in bins (UQ7.9). */
	int8_t   calTemp; /*!< The last BDV calibration temperature in degree C (calibTemp-3 <= currTemp <= calibTemp+3). */
	int8_t   force20MhzVcselTemp; /*!< The temperature at which the measurement needs to run with 20MHz due to a weak VCSEL driver. */
	int8_t   tdcBinCalibrationQ9[5]; /*!< The TDC bin width electrical calibration value in Q9. `binWidth = normWidth * (1 + calibValue)`. -128..75%, 0..0%, 127..24.6%. */
};
typedef struct _tmf8806StateData tmf8806StateData;

/*! The factory calibration data for the optical crosstalk. */
struct __attribute__((packed)) _tmf8806FactoryCalibData
{
	uint32_t id:4; /*!< The factory data ID. Must be 0x2, or the state data will be discarded. */
	uint32_t crosstalkIntensity:20; /*!< The crosstalk intensity value. */
	uint32_t crosstalkTdc1Ch0BinPosUQ6Lsb:8; /*!< The first crosstalk bin position as UQ6.6 (lower 8 bits). */

	uint32_t crosstalkTdc1Ch0BinPosUQ6Msb:4; /*!< The first crosstalk bin position as UQ6.6 (upper 4 bits). */
	uint32_t crosstalkTdc1Ch1BinPosDeltaQ6:9; /*!< The crosstalk bin position delta to bin pos 0 as Q2.6. */
	uint32_t crosstalkTdc2Ch0BinPosDeltaQ6:9; /*!< The crosstalk bin position delta to bin pos 0 as Q2.6. */
	uint32_t crosstalkTdc2Ch1BinPosDeltaQ6:9; /*!< The crosstalk bin position delta to bin pos 0 as Q2.6. */
	uint32_t crosstalkTdc3Ch0BinPosDeltaQ6Lsb:1; /*!< The crosstalk bin position delta to bin pos 0 as Q2.6 (lower 1 bits). */

	uint32_t crosstalkTdc3Ch0BinPosDeltaQ6Msb:8; /*!< The crosstalk bin position delta to bin pos 0 as Q2.6 (upper 8 bits). */
	uint32_t crosstalkTdc3Ch1BinPosDeltaQ6:9; /*!< The crosstalk bin position delta to bin pos 0 as Q2.6. */
	uint32_t crosstalkTdc4Ch0BinPosDeltaQ6:9; /*!< The crosstalk bin position delta to bin pos 0 as Q2.6. */
	uint32_t crosstalkTdc4Ch1BinPosDeltaQ6Lsb:6; /*!< The crosstalk bin position delta to bin pos 0 as Q2.6 (lower 6 bits). */

	uint8_t crosstalkTdc4Ch1BinPosDeltaQ6Msb:3; /*!< The crosstalk bin position delta to bin pos 0 as Q2.6 (upper 3 bits). */
	uint8_t reserved:5; /*!< Reserved for future use. */

	uint8_t opticalOffsetQ3; /*!< The optical system offset. */
};
typedef struct _tmf8806FactoryCalibData tmf8806FactoryCalibData;

#endif /* INCLUDE_IN_ASM == 0 */

#endif /* TMF8806_REGS_H */
