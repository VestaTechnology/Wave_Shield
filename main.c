/* 
 * File:   main.c
 * Author: JBurnworth
 *
 * Created on December 17, 2014, 3:21 PM
 *-----------------------------------------------------------------------
 * This project mounts an SD Card with Fat Formating and looks for wav
 * files in the root directory.  It will try to play those files using
 * functions in waveReader.c.  This project implements a Petit FATFs
 * module with directory, read and seek functionality enabled.  Data
 * forwarding is not implemented.
 *
 * The modules in this project make Mercury18 compatible with an
 * Adafruit wavShield for arduino.
 *
 * https://learn.adafruit.com/adafruit-wave-shield-audio-shield-for-arduino/overview
 *-----------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <xc.h>
#include <usart.h>
#include <spi.h>
#include "../mercury18.h"
#include "diskio.h"
#include "integer.h"
#include "pff.h"
#include "pffconf.h"
#include "waveReader.h"
#include "config_bits.h"

/* Set the configuration bits:
 * - No extended instruction set
 * - Oscillator = HS
 * - PLL = 4x
 * - SOSC set to digital => PORTC.1 and PORTC.0 are I/O
 */
#pragma config XINST = 0
#pragma config FOSC = HS2
#pragma config PLLCFG = 1
#pragma config SOSCSEL = 2

/*-----------------------------------------------------------------------
 * Open the COM port to talk to a computer terminal and make pins used in
 * SPI bit-bang outputs.
 *-----------------------------------------------------------------------*/
void init_COM (unsigned short baud_rate) {
    /* Serial port, etc. initialization */
    ANCON2 = 0;
    TRISG &= 0xFD;
    TRISG |= 0x04;
    Open2USART(USART_RX_INT_OFF
            & USART_TX_INT_OFF
            & USART_ASYNCH_MODE
            & USART_EIGHT_BIT
            & USART_CONT_RX
            & USART_BRGH_HIGH, baud_rate);

    // Set the pins used to bit-bang spi to the DAC as outputs.
    // See waveReader.h for definitions
    dacCsTris = 0;
    dacSckTris = 0;
    dacSdiTris = 0;
}

int main() {
    init_COM(BAUD_38400);
    BYTE res;                   // Holds function return/error vaules
    FATFS fs;			// File system object

// <editor-fold defaultstate="collapsed" desc="DEBUG - play middle C">
//    Code used in debugging, plays a slightly flat middle C
//    BYTE goingUp = 1;
//    WORD step = 1560;
//    timer2_ON();
// </editor-fold>

    printf("%cc",0x1B);         // Reset COM Terminal
    while (1) {
        res = pf_mount(&fs);        // Mount SD card
        if (res == FR_OK) {
            printf("SD card initialized succesfully: ");
            put_rc(res);
            OpenSPI1(SPI_FOSC_4,MODE_00,SMPMID);    // Reinitialize SPI to fastest speed for maximum data rates with SD Card
        } else {
            printf("Error initializing SD card.");
            put_rc(res);
        }
        // As long as the sd was mounted successfully, play files in root over and over
        while (res == FR_OK) {
            BYTE wavRes;
            wavRes = rootPlay();
            // if rootPlay returned an error, print it and break loop
            if (wavRes != FR_OK) {
                printf("break while_main;");
                put_rc(wavRes);
                break;
            }

    // <editor-fold defaultstate="collapsed" desc="DEBUG - play middle C">
    //    ****See interruptDAC.h/.c for more debug code****
    //  Code used in debugging, plays a slightly flat middle C
    //            if (update) {
    //                if (goingUp) {
    //                    middleC += step;
    //                } else {
    //                    middleC -= step;
    //                }
    //                if (goingUp && middleC < step) {
    //                    goingUp = 0;
    //                    middleC = 0xFFFF;
    //                } else if (!goingUp && middleC > (0xFFFF-step)) {
    //                    goingUp = 1;
    //                    middleC = 0;
    //                }
    //                update = 0;
    //            }
    // </editor-fold>
        }
    }

    return (EXIT_SUCCESS);
}