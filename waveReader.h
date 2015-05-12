/* 
 * File:   waveReader.h
 * Author: JBurnworth
 *
 * Created on January 20, 2015, 3:42 PM
 */

#ifndef WAVEREADER_H
#define	WAVEREADER_H

#ifdef	__cplusplus
extern "C" {
#endif
    
#include "pff.h"

#define bufflen 512

typedef enum {
    SD_LAST_BUFFER = 0,
    SD_READY,
    SD_FILLING
} SDSTATUS;

// <editor-fold defaultstate="collapsed" desc="DEBUG - play middle C">
//  Code used in debugging, plays a slightly flat middle C
//extern BYTE update;
//extern WORD middleC;
// </editor-fold>

/* Define lables for pins used to interface with the DAC. */
#define dacCsTris TRISBbits.TRISB0
#define dacCs PORTBbits.RB0
#define dacSckTris TRISBbits.TRISB1
#define dacSck PORTBbits.RB1
#define dacSdiTris TRISBbits.TRISB2
#define dacSdi PORTBbits.RB2

/* Define macros to bit-bang mode 0.0 spi to the DAC. */
#define bitMask(n) (1<<(n))

#define dacCsLow() dacCs = 0
#define dacCsHigh() dacCs = 1

#define dacSckLow() dacSck = 0
#define dacSckHigh() dacSck = 1
#define dacSckPulse() {dacSckHigh(); dacSckLow();}

#define dacSdiSet(b) if(b) {dacSdi = 1;} else {dacSdi = 0;}

/* Send bit j of jays */
#define dacSendBit(j, jays) {dacSdiSet(jays & bitMask(j)); dacSckPulse();}

#define dacSendOne() {dacSdi = 1; dacSckPulse();}
#define dacSendZero() {dacSdi = 0; dacSckPulse();}

/*---------------------------------------*/
/* Prototypes for disk control functions */
void put_rc (FRESULT rc);
FRESULT rootPlay(void);
FRESULT openWav(const char* fname);
FRESULT playWav(void);
void interrupt dacInterrupt(void);

#ifdef	__cplusplus
}
#endif

#endif	/* WAVEREADER_H */

