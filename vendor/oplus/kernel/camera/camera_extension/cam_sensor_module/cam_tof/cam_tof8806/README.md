# TMF8806 Linux Reference Driver README.md

## Description

This file describes the installation of the tmf8806 linux reference driver, and how to use it with the Sysfs.

### Compilation

    The compilition was done with the ams-Osram provided image bookworm_1v0.

    Linux version 6.1.0-rpi7-rpi-v6 (debian-kernel@lists.debian.org) (gcc-12 (Raspbian 12.2.0-14+rpi1) 12.2.0,
    GNU ld (GNU Binutils for Raspbian) 2.40) #1 Raspbian 1:6.1.63-1+rpt1 (2023-11-24)

## Files
    
    Overlay Files
    The device tree overlay files for the tmf8806 prototyped on the Raspberry Pi zero.
        - tmf8806-overlay.dts
          pinheader configuration, EN gpio and INT gpio
        - tmf8806-overlay-fpc.dts
          evm configuration with flex printed circuit, EN gpio and INT gpio
        - tmf8806-overlay-polled.dts
          pinheader configuration, EN gpio and INT register polled
        - tmf8806-overlay-polled-fpc.dts
          evm configuration with flex printed circuit, EN gpio and INT register polled

    Module File
        - tmf8806.ko    

## Installation 

Configuration of Raspberry Pi zero:
1. Use the right image

   bookworm_1v0 (provided by ams-Osram)

2. connect to Raspberry Pi

    ssh ams@169.254.0.2
    Pw: ams
	
3. copy the right files on the Raspberry Pi 
   
   - Module tmf8806.ko
   
   - tmf8806_install.sh for fpc(EVM Demo), tmf8806_install_pinheader.sh for pinheader

   - Overlay file tmf8806-overlay.dtbo for pinheader or tmf8806-overlay-fpc.dtbo for fpc (EVM Demo)
   
4. chmod 777 tmf8806_install_pinheader.sh
   
5. sudo su (Super User mode)
   
6. ./tmf8806_install_pinheader.sh for pinheader or ./tmf8806_install.sh for fpc 

7. sync

8. reboot (reboot system)

## General commands on the Raspberry Pi

1. Connect to Raspberry Pi

    - ssh ams@169.254.0.2
    - Pw: ams

2. See pinout on Raspberry Pi

    - pinout

3. Debug messages

    - list: dmesg
    - list and clear: sudo dmesg -c
   
4. I2C commands on command line

    - Detect i2c devices: i2cdetect -y 1
    - I2C Read:           i2cget -y 1 0x41 0xE0
    - I2C Write:          i2cset -y 1 0x41 0xE0 0x01

6. Change Raspberry I2C frequency

    - in file: /boot/config.txt
    - reboot

5. Module Functions

    - list all modules: lsmod
    - insert module:    sudo insmod tmf8806.ko
    - remove module:    sudo rmmod tmf8806.ko
    - module info :     modinfo tmf8806.ko

6. See running tmf8806 thread for interrupt handling

    - ps -ef

7. Makefile command for TMF8806

    - make CONFIG_SENSORS_TMF8806=m

8. The overlay file could be changed

    - copy overlay file to boot directory
    
    - if overlay files are generated 

        - cp /home/ams/arch/arm/boot/dts/tmf8806-overlay-polled.dtbo /boot/overlays 
        - cp /home/ams/arch/arm/boot/dts/tmf8806-overlay.dtbo /boot/overlays 
        - cp /home/ams/arch/arm/boot/dts/tmf8806-overlay-polled-fpc.dtbo /boot/overlays 
        - cp /home/ams/arch/arm/boot/dts/tmf8806-overlay-fpc.dtbo /boot/overlays 

    - in /boot/config.txt add the desired overlay file:

        - dtoverlay=tmf8806-overlay-polled
        - dtoverlay=tmf8806-overlay
        - dtoverlay=tmf8806-overlay-polled-fpc
        - dtoverlay=tmf8806-overlay-fpc

    - echo "dtoverlay=tmf8806-overlay" >> /boot/config.txt

   
## System File System

- I2C Bus 1 (pinheader): /sys/class/i2c-adapter/i2c-1/1-0041/
- I2C Bus 0 (fpc): /sys/class/i2c-adapter/i2c-0/0-0041/

A value with 0x prepended means the attribute interpretation is done in hexadecimal.

### Common Attributes (tmf8806_common)

1. chip_enable (R/W)
    
    - See chip enabled status: cat chip_enable
    - Enable the device:       echo 1 > chip_enable
    - Disable the device:      echo 0 > chip_enable

2. driver_debug (R/W)
      
    - Debugging statement for driver
        - 0x0    ... no logging
        - 0x1    ... only error logging
        - 0x8    ... this is a bit-mask check for clock correction logging
        - 0x10 ... some information
        - 0x20 ... very chatty firmware
        - 0x80 ... this is a bit-mask check for i2c logging
        - 0xFF ... dump everything

    - Show debug state (hex):  cat driver_debug
    - Enable debugging:  echo 0x1 > driver_debug
    - Disable debugging: echo 0x0 > driver_debug

3. program (R/W)
      
    - See running program (hex):   cat program
    - Switch to Bootloader:  echo 0x80 > program
    - Switch to App0:        echo 0xC0 > program
    
4. program_version (R)
    
    - See program, major/minor/patch version, chip id and chip revision (hex):
    cat program_version
      
5. registers (R)
    
    - Dump Registers 0x00-0xFF (hex): cat registers
       
6. register_write (W)
    
    - Write to device register:  echo "0xE0:0x41" > register_write
    - example stop measurement:  echo "0x10:0xFF" > register_write
    - example irq enabling:      echo "0xe2:0x01" > register_write
                  
7. request_ram_patch(W)

    The firmware mainapp_PATCH_Maxwell.hex located in /lib/firmware will be downloaded
    - Download Firmware patch: echo 1 > request_ram_patch
    

### Application Attributes (tmf8806_app0)

See datasheet for the measurement command parameters: 
- spreadSpecSpad
- spreadSpecVcsel
- data_setting
- alg_setting
- gpio_setting
- capture_delay
- snr

Attributes:

1. alg_setting (R/W)
      
    - Show algorithm setting of app0_command (hex): cat alg_setting
    - Set algorithm setting of app0_command: echo 0x2 > alg_setting
      
2. app0_apply_fac_calib  (W)
      
    - Perform a factory calibration: echo 1 > app0_apply_fac_calib
        
3. app0_clk_correction  (W)
    
    - Disable Clock Correction: echo 0 > app0_clk_correction
    - Enable  Clock Correction: echo 1 > app0_clk_correction
    
4. app0_command (R/W)
      
    - See the registers from 0x06 to 0x10.
      Show app0_command: cat app0_command
      
    - Write a commando last value is the commando at register 0x10.
      DO NOT USE!! I2C command only, no logic in driver is used.      
      Set app0_command: echo 0x0 0x0 0x3 0x2 0x0 0x0 0x6 0x21 0x90 0x1 0x2 > app0_command
5. app0_ctrl_reg( R )

    - Show registers  0x00 - 0x20 (hex): cat app0_ctrl_reg

6. app0_fac_calib (R/W)

    - Show factory calibration data (hex): cat app0_fac_calib
    - Set factory calibration data: echo 0x3 0x0 0x0 0xf3 0xaf 0x7f 0xc1 0x3 0xf6 0xea 0x27 0x18 0x0 0x4 > app0_fac_calib

7. app0_histogram_readout (W)
    - Modes
      - TMF8806_DUMP_HIST_ELECTRICAL_CAL     0x1
      - TMF8806_DUMP_HIST_PROXIMITY          0x2
      - TMF8806_DUMP_HIST_DISTANCE           0x4
      - TMF8806_DUMP_HIST_ALG_PILEUP         0x8
      - TMF8806_DUMP_HIST_ALG_PU_TDC_SUM     0x10
    - Configuration of histogram readout: echo 0x1 > app0_histogram_readout

8. app0_state_data (W)
    
    - Set the state data:   echo 0x2 0x3c 0x0 0x0 0x7f 0x7f 0x0 0x0 0x0 0x0 0x0 > app0_state_data
        
9. capture (R/W)

    - Show  app0_command (hex):          cat capture
    - Stop  capture of app0_command: echo 0 > capture
    - Start capture of app0_command: echo 1 > capture

10. capture_delay (R/W)
    
    - Show capture delay of app0_command (dec): cat capture_delay 
    - Show capture delay of app0_command (dec): echo 1 > capture_delay
    
11. data_setting (R/W)

    - Show data setting of app0_command (hex): cat data_setting
    - Set data setting of app0_command:  echo 0x2 > data_setting

12. gpio_setting (R/W)

    - Show gpio setting of app0_command (hex): cat gpio_setting 
    - Set gpio setting of app0_command:  echo 0x1 > gpio_setting
      
13. iterations (R/W)
    
    - Show iterations of app0_command (dec): cat iterations
    - Set iterations of app0_command (dec): echo 500 > iterations
    
14. period (R/W)
    
    - Show period of app0_command (dec):  cat period
    - Set period of app0_command (dec):  echo 33 > period

15. snr (R/W)
    
    - Show snr of app0_command (hex): cat snr
    - Set snr of app0_command:  echo 0x6 > snr
     
16. spreadSpecSpad (R/W)

    - Show spreadSpecSpad of app0_command (hex): cat spreadSpecSpad
    - Set spreadSpecSpad of app0_command:  echo 0x0 > spreadSpecSpad
    
17. spreadSpecVcsel (R/W)

    - Show spreadSpecVcsel of app0_command (hex): cat spreadSpecVcsel
    - Set  spreadSpecVcsel of app0_command: echo 0x0 > spreadSpecVcsel
    
18. distance_thresholds (R/W)
    
    - Show persistance, low and high threshold (dec): cat distance_thresholds
    - Set  persistance, low and high threshold: echo 0 0 2710 > distance_thresholds

19. app0_osc_trim (R/W)
    - Show oscillator trim value (dec): cat app0_osc_trim
    - Set oscillator trim value (dec):  echo 100 > app0_osc_trim

### Output data

1. app0_tof_output (R)

    Every data frame has 4 four byte header.
      Byte[0] = FrameID (Histogram-Type = PileUp, EC, Raw, Long, Short, Result=0x55)
      Byte[1] = frameNumber (free-running number, increments with each frame)
      Byte[2] = payload_size_LSB
      Byte[3] = payload_size_MSB

    Result Data from I2C readout starting from register 0x1C. For content see datasheet.
    If histogram readout is enabled, the raw histograms are also dumped.
    
    Frame ID:
      TMF8806_DIAG_HIST_ELECTRICAL_CAL                    1        
      TMF8806_DIAG_HIST_PROXIMITY                         4  
      TMF8806_DIAG_HIST_DISTANCE                          7    
      TMF8806_DIAG_HIST_ALG_PILEUP                        16   
      TMF8806_DIAG_HIST_ALG_PU_TDC_SUM                    17   

    - Output data (binary) could be read: cat app0_tof_output
    
    Note: The output buffer is a fifo buffer with 32k.





