/*
 * File:   waveReader.c
 * Author: JBurnworth
 *
 * Created on January 20, 2015, 3:42 PM
 *-----------------------------------------------------------------------
 * Functions to validate and read WAV Files, and send the data to the
 * DACAmp on the WAVShield with an Interrupt Service Routine.
 *-----------------------------------------------------------------------*/

#include <xc.h>
#include <stdio.h>
#include <string.h>
#include <adc.h>
#include "pff.h"
#include "waveReader.h"


//#define USE_OR_MASKS // For XC8 peripheral libraries (OpenADC())


static BYTE buffer1[bufflen];
static BYTE buffer2[bufflen];

static BYTE bitsPerSample;

static SDSTATUS status;
static BYTE wavFormatGood = 0;

static BYTE* playPos;
static BYTE* playEnd;
static BYTE* playBuff;
static BYTE* buffEnd;

static DWORD bytesPlayed;



/*-----------------------------------------------------------------------
 * Print the FRESULT of function return.  FRESULTs are used to carry and
 * and check for errors with many functions. See pff.h for FRESULT values.
 *-----------------------------------------------------------------------*/
void put_rc (FRESULT rc)
{
	const char *p;
	FRESULT i;
	static const char str[] =
		"OK\0DISK_ERR\0NOT_READY\0NO_FILE\0NOT_OPENED\0NOT_ENABLED\0"
                "NO_FILE_SYSTEM\0NOT_WAV_FILE\0WAV_TYPE_UNSUPPORTED\0WAV_END\0";

	for (p = str, i = 0; i != rc && *p; i++) {
		while(*p++) ;
	}
	printf("\trc=%i FR_%s\n\r", rc, p);
        return;
}

/*-----------------------------------------------------------------------
 * Attempts to play all files located in the root directory of passed file
 * system.  Will continue playing root directory in a loop. Returns FRESULT
 * indicating success or (which) failure.
 *-----------------------------------------------------------------------*/
FRESULT rootPlay(void)
{
    BYTE res;
    DIR dir = {0};		/* Directory object */
    FILINFO fno = {0};		/* File information */
    char path[128] = {NULL};    /* Set path to root directory */
    
    /* Open root directory, if error opening, print and break. */
    res = pf_opendir(&dir, path);
    if (res) {
        printf("opendir error; ");
        return res;
    }
    /* This WHILE loop attempts to read all the files in the root directory.
     * If the file opens succesfully, it prints the name to console,
     * and sends the file to openWav. */
    while(1) {
        res = pf_readdir(&dir, &fno);
        if (res != FR_OK) { put_rc(res); break; }   // break because opening directory failed
        if (fno.fname[0] == 0) {
            printf("End of directory\n\n\r");
            break;               // break because all entries read
        }

        if (fno.fattrib & AM_DIR) { // A DIRECTORY was read
            printf("   <DIR>   %s\n\r", fno.fname);   // print directory name
        } else {                    // A FILE was read
            /* Attempt to open the file and if it is the correct format, play it. */
            res = openWav(fno.fname);
            if (res == 0) {
                BYTE playRes;
                printf("Playing %s\n\r", fno.fname);
                playRes = playWav();    // Note, playWay is only called if openWav returned successfully
                if (playRes != FR_WAV_END) put_rc(playRes);
            } else {
                printf("Failed opening %s as a wav file\n\r", fno.fname);
                put_rc(res);
            }
        }
    }
    return res;
}

/*-----------------------------------------------------------------------
 * openWav attempts to open the file passed to it (fname).  openWav looks
 * for a wav file format and checks that the format meets limits imposed by
 * hardware.  Returns FRESULT indicating success or (which) failure.
 *-----------------------------------------------------------------------*/
FRESULT openWav(const char* fname)
{
    BYTE res;
    const UINT wavHeaderLen = 12;
    const UINT fmtHeaderLen = 8;
    UINT bReadCount;            /* Number of bytes read by pf_read() */

    union {
        struct {
            BYTE id[4];
            DWORD size;
            BYTE data[4];
        } riff;  // Wave Header
        struct {
            UINT compress;
            UINT channels;
            DWORD sampleRate;
            DWORD bytesPerSecond;
            UINT blockAlign;
            UINT bitsPerSample;
            UINT extraBytes;
        } fmt; // fmt chunk
    } buf;

    // This file must be verified before we set wavFormatGood to True
    wavFormatGood = 0;

    res = pf_open(fname);        // attempt to open file
    if (res != 0) {              // Error opening file
        printf("%s failed to open\n\n\r", fname);
        return res;
    }
    /* Read the WAVE file header and check for correctness. */
    res = pf_read(&buf, wavHeaderLen, &bReadCount);
    if (res != 0) return res;                  // File read error 
    if (bReadCount != wavHeaderLen             // Unexpected number of bytes read
        || strncmp(buf.riff.id, "RIFF", 4)     // Incorrect header
        || strncmp(buf.riff.data, "WAVE", 4))  // Incorrect header
    {
        return FR_NOT_WAV_FILE;
    }
    
    /* Read the FORMAT chunk header and check for correctness. */
    res = pf_read(&buf, fmtHeaderLen, &bReadCount);
    if (res != 0) return res;                   // File read error 
    if (bReadCount != fmtHeaderLen              // Unexpected number of bytes read
        || strncmp(buf.riff.id, "fmt ", 4))     // Incorrect header (note ending space)
    {
        return FR_NOT_WAV_FILE;
    }
    /* Read FORMAT chunk (if it's 16 or 18 bytes long) and check for wave format compatibility. */
    UINT size = buf.riff.size;
    if (size == 16 || size == 18) {
        res = pf_read(&buf, size, &bReadCount);
        if (res != 0) return res;               // File read error 
        if (bReadCount != size) {               // Unexpected number of bytes read
            return FR_NOT_WAV_FILE;
        }
    } else {
        printf("FORMAT chunk not 16 or 18 bytes long.\n\r");
        return FR_WAV_TYPE_UNSUPPORTED;
    }

    /* Check format chunk to make sure file is supported by this program and wavsheild. */
    /* The print messages explain the conditions being tested for. */
    if (buf.fmt.compress != 1 || (size == 18 && buf.fmt.extraBytes != 0)) {
        printf("Compression not supported\n\r");
        return FR_WAV_TYPE_UNSUPPORTED;
    }
    if (buf.fmt.channels > 1) {
        printf("Not mono\n\r");
        return FR_WAV_TYPE_UNSUPPORTED;
    }
    bitsPerSample = buf.fmt.bitsPerSample;
    if (bitsPerSample > 16) {
        printf("More than 16 bits per sample!\n\r");
        return FR_WAV_TYPE_UNSUPPORTED;
    }
    if (buf.fmt.bytesPerSecond > 44100){
        printf("Sample rate too fast.\n\r");
        return FR_WAV_TYPE_UNSUPPORTED;
    }

    // We made, it's a wav file with appropriate formating
    wavFormatGood = 1;      // wavFormatGood is true
    return res;
}

// <editor-fold defaultstate="collapsed" desc="DEBUG - play middle C">
//   Code used in debugging, plays a slightly flat middle C
//WORD middleC = 0;
//BYTE update = 0;
// </editor-fold>

/*-----------------------------------------------------------------------
 * playWav should only be called after a successful openWav call, see
 *                  variable 'wavFormatGood'.
 *
 * playWav finds the beginning of the wav files data chunck, preemptively
 * loads both buffers and initiates playing by opening timer2.  Then
 * playWav goes into a while loop monitoring for end of file and the need
 * to refill a buffer.
 *-----------------------------------------------------------------------*/
FRESULT playWav(void)
{
    // if openWav hasn't been run successfully, dont try to play the file
    if (wavFormatGood != 1) {
        return FR_NOT_READY;
    }

    BYTE res;
    UINT bReadCount;            // Number of bytes read
    const UINT dataHeaderLen = 8;
    struct {
        BYTE id[4];     // Chunk ID
        DWORD size;     // Chunk size
    } header;

// <editor-fold defaultstate="collapsed" desc="Change sample rate">
//    OpenADC(ADC_FOSC_4 & ADC_LEFT_JUST & ADC_4_TAD, ADC_CH0 & ADC_INT_OFF, ADC_REF_VDD_VSS);
// </editor-fold>
    // Read chunk headers until we are at the data chunk
    while (1) {
        res = pf_read(&header, dataHeaderLen, &bReadCount);
        if (res != 0) return res;                   // File read error
        if (bReadCount == dataHeaderLen             // check number of bytes read
                && !strncmp(header.id, "data", 4))  // We are looking for the data chunk
        {
            // Stop reading headers and go fill those buffers!
            printf("Data chunk size: %lu\n\r", header.size);
            break;
        } else {
            // Chunk isn't 'data', jump over it and check next chunk
            res = pf_jump(header.size);
            if (res != 0) return res;
        }
    }

    // Atempt to fill buffer1.  Return res if read was not successful
    // Return FR_WAV_END if zero bytes are read into buffer1
    res = pf_read(&buffer1, bufflen, &bReadCount);
    if (res != 0) return res;                   // File read error
    if (bReadCount == 0) return FR_WAV_END;
    // Set playPos as first index, and playEnd as last index of buffer1
    playPos = buffer1;
    playEnd = buffer1 + bReadCount;

    // Atempt to fill buffer2.  Return res if read was not successful
    res = pf_read(&buffer2, bufflen, &bReadCount);
    if (res != 0) return res;                   // File read error
    // Set playBuff as first index, and buffEnd as last index of buffer2
    playBuff = buffer2;
    buffEnd = buffer2 + bReadCount;
    // Initialize status and bytesPlayed for beginning of file
    status = SD_READY;
    bytesPlayed = 0;

    // Opening timer2 with interrupts begins the playing proccess!
    // Initialize period regiter of timer2 with 180 for 22050 Hz
    // playback.  Vaule verified emperically with Fosc = 32 MHz
    OpenTimer2(TIMER_INT_ON & T2_PS_1_1 & T2_POST_1_2);
    INTCON = 0xC0;          // Enable Peripheral and Global interrupts
    PR2 = 180;          // Changing this alters playing rate!!!

    // This while loop keeps the music playing and buffers filling until the
    // number of bytes played is equal to or great than the header size.
    // The next buffer is filled based on the SD_FILLING flag, set in the ISR
    // when the double buffers are switched.
    while (1) {
        // <editor-fold defaultstate="collapsed" desc="Change sample rate">
        //SelChanConvADC(ADC_CH0);
        //char adc0 = (ReadADC()/273) + 160;
        //PR2 = adc0;
        // </editor-fold>
        // If end of file, close timer2, set good return value and break while loop
        if (bytesPlayed >= header.size) {
            CloseTimer2();
            res = FR_WAV_END;
            break;
        }
        // SD_FILLING flag set in ISR
        if (status == SD_FILLING) {
            // swap double buffers
            playBuff = playBuff != buffer1 ? buffer1 : buffer2;
            res = pf_read(playBuff, bufflen, &bReadCount);  // Refill playBuff
            if (res != 0) return res;                       // File read error
            buffEnd = playBuff + bReadCount;        // more swapping logic
            // clear flag so we wont be back here until another buffer is read
            status = SD_READY;
        }
    }

    return res;
}

/*-----------------------------------------------------------------------
 *                         ******* ISR *******
 * First, checks for and handles double buffer swapping and sets flag to
 * refil buffer. Then, loads the current sample and sends it to DAC over a bit
 * bang SPI.  Also, increments bytes played counter (used to end file play).
 *-----------------------------------------------------------------------*/
void interrupt dacInterrupt(void)
{
    if (PIR1bits.TMR2IF) {
        // Check if we're at the end of our playing buffer
        if (playPos >= playEnd) {
            // Swap double buffers and set flag to fill playBuff
            playPos = playBuff;
            playEnd = buffEnd;
            status = SD_FILLING;
            }

        BYTE sampleH, sampleL;

        // Load current sample to send to DAC, progress current sample position
        // and bytes played counter by respective amounts
        if (bitsPerSample == 16) {  // 16-bit sample
            sampleH = 0x80 ^ playPos[1];    // 16-bit is signed
            sampleL = playPos[0];
            playPos += 2;                   // Move to next play position
            bytesPlayed += 2;
        } else {        // 8-bit samples
            sampleH = playPos[0];           //8-bit is unsigned
            sampleL = 0;
            playPos += 1;                   // Move to next play position
            bytesPlayed += 1;
        }

        dacCsLow();         // Active DAC with low chip select

        dacSendZero();      // Address DAC A (not B)
        dacSendZero();      // Use DAC in unbuffered mode
        dacSendOne();       // 1X gain
        dacSendOne();       // Not in shutdown mode
        /* Send high 8 bits. */
        dacSendBit(7, sampleH);
        dacSendBit(6, sampleH);
        dacSendBit(5, sampleH);
        dacSendBit(4, sampleH);
        dacSendBit(3, sampleH);
        dacSendBit(2, sampleH);
        dacSendBit(1, sampleH);
        dacSendBit(1, sampleH);
        /* Send low 4 bits, dropping the 4 LSbs. */
        dacSendBit(7, sampleL);
        dacSendBit(6, sampleL);
        dacSendBit(5, sampleL);
        dacSendBit(4, sampleL);

        dacCsHigh();        // Chip select high - done

// <editor-fold defaultstate="collapsed" desc="DEBUG - play middle C">
//  Code used in debugging, plays a slightly flat middle C
// Code to send a middle C (~261 Hz) for debugging
//        dacCsLow();         // Active DAC with low chip select
//
//        dacSendZero();      // Address DAC A (not B)
//        dacSendZero();      // Use DAC in unbuffered mode
//        dacSendOne();       // 1X gain
//        dacSendOne();       // Not in shutdown mode
//        /* Send high 8 bits. */
//        dacSendBit(15, middleC);
//        dacSendBit(14, middleC);
//        dacSendBit(13, middleC);
//        dacSendBit(12, middleC);
//        dacSendBit(11, middleC);
//        dacSendBit(10, middleC);
//        dacSendBit(9, middleC);
//        dacSendBit(8, middleC);
//        /* Send low 4 bits, dropping the 4 LSbs. */
//        dacSendBit(7, middleC);
//        dacSendBit(6, middleC);
//        dacSendBit(5, middleC);
//        dacSendBit(4, middleC);
//
//        dacCsHigh();        // Chip select high - done
//        update = 1;
// </editor-fold>
        // Clear interrupt flag, now another interrupt can occur
        PIR1bits.TMR2IF = 0;
    }
}