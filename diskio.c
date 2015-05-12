/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for Petit FatFs (C)ChaN, 2014      */
/*-----------------------------------------------------------------------*/

#include <xc.h>
#include <spi.h>
#include <stdio.h>
#include "diskio.h"

#define SELECT()    LATC2 = 0
#define DESELECT()  LATC2 = 1

/* Definitions for MMC/SDC command */
#define CMD0	(0x40+0)	/* GO_IDLE_STATE */
#define CMD1	(0x40+1)	/* SEND_OP_COND (MMC) */
#define	ACMD41	(0xC0+41)	/* SEND_OP_COND (SDC) */
#define CMD8	(0x40+8)	/* SEND_IF_COND */
#define CMD16	(0x40+16)	/* SET_BLOCKLEN */
#define CMD17	(0x40+17)	/* READ_SINGLE_BLOCK */
#define CMD24	(0x40+24)	/* WRITE_BLOCK */
#define CMD55	(0x40+55)	/* APP_CMD */
#define CMD58	(0x40+58)	/* READ_OCR */


/* Card type flags (CardType) */
#define CT_MMC				0x01	/* MMC ver 3 */
#define CT_SD1				0x02	/* SD ver 1 */
#define CT_SD2				0x04	/* SD ver 2 */
#define CT_BLOCK			0x08	/* Block addressing */

static BYTE CardType;

/*-----------------------------------------------------------------------
 * Initialize SPI 1 at a slow speed to begin talking to the SD card.
 *-----------------------------------------------------------------------*/
static void init_spi()
{
        OpenSPI1(SPI_FOSC_64,MODE_00,SMPMID);
        TRISC2 = 0;
}

/*-----------------------------------------------------------------------
 * A short delay used in opening communication with the SD card.
 *-----------------------------------------------------------------------*/
void dly_100us()
{
        for(int i=800;i>=0;i--) {
            continue;
        }
}

/*-----------------------------------------------------------------------
 * Writes 'data_out' to SPI 1
 *-----------------------------------------------------------------------*/
static void write_spi(BYTE data_out)
{
        unsigned char TempVar;
        TempVar = SSP1BUF;          // Clear BufferFull
        PIR1bits.SSP1IF = 0;        // Clear interrupt flag
        SSP1BUF = data_out;         // write byte to SSP1BUF register to initiate transfer
        while(!PIR1bits.SSP1IF);    // wait until bus cycle complete
}

/*-----------------------------------------------------------------------
 * Read and return the value of SPI 1's buffer.
 *-----------------------------------------------------------------------*/
static BYTE read_spi()
{
        unsigned char TempVar;
        TempVar = SSP1BUF;          // Clear BufferFull
        PIR1bits.SSP1IF = 0;        // Clear interrupt flag
        SSP1BUF = 0xFF;             // write byte to SSP1BUF register to initiate transfer
        while(!PIR1bits.SSP1IF);    // wait until bus cycle complete
        return ( SSP1BUF );         // return with byte read
}

/*-----------------------------------------------------------------------*/
/* Logic to handle ACMD vs regular CMD.  Calls cmd_xfer which sends the  */
/* actual command.                                                       */
/*-----------------------------------------------------------------------*/
static
BYTE send_cmd(
	BYTE cmd,		/* 1st byte (Start + Index) */
	DWORD arg		/* Argument (32 bits) */
)
{
        BYTE n, res;

        if (cmd & 0x80) {	/* ACMD<n> is the command sequense of CMD55-CMD<n> */
                cmd &= 0x7F;
                res = cmd_xfer(CMD55, 0);
                if (res > 1) return res;  /* If there is an error, return it */
                res = cmd_xfer(cmd, arg);
                return res;
        } else {
                res = cmd_xfer(cmd, arg);
                return res;
        }
        

}
/*-----------------------------------------------------------------------*/
/* Send a single command packet to the SDC/MMC                           */
/*-----------------------------------------------------------------------*/
static
BYTE cmd_xfer (
	BYTE cmd,		/* 1st byte (Start + Index) */
	DWORD arg		/* Argument (32 bits) */
)
{
	BYTE n, res;

	/* Select the card */
	DESELECT();
	read_spi();
	SELECT();
	read_spi();

	/* Send a command packet */
	write_spi(cmd);						/* Start + Command index */
	write_spi((BYTE)(arg >> 24));		/* Argument[31..24] */
	write_spi((BYTE)(arg >> 16));		/* Argument[23..16] */
	write_spi((BYTE)(arg >> 8));			/* Argument[15..8] */
	write_spi((BYTE)arg);				/* Argument[7..0] */
	n = 0x01;							/* Dummy CRC + Stop */
	if (cmd == CMD0) n = 0x95;			/* Valid CRC for CMD0(0) */
	if (cmd == CMD8) n = 0x87;			/* Valid CRC for CMD8(0x1AA) */
	write_spi(n);

	/* Receive a command response */
	n = 10;								/* Wait for a valid response in timeout of 10 attempts */
	do {
		res = read_spi();
	} while ((res & 0x80) && --n);

	return res;			/* Return with the response value */
}

/*-----------------------------------------------------------------------*/
/* Initialize Disk Drive                                                 */
/*-----------------------------------------------------------------------*/
DSTATUS disk_initialize (void)
{
    	BYTE n, cmd, ty, ocr[4];
	UINT tmr;

	init_spi();		/* Initialize ports to control SDC/MMC */
	DESELECT();
	for (n = 10; n; n--) read_spi();	/* 80 Dummy clocks with CS and DI High */

	ty = 0;
	if (send_cmd(CMD0, 0) == 1) {			/* Enter Idle state */
                //printf("Enter Idle State\n\r");   //debug printing
		if (send_cmd(CMD8, 0x1AA) == 1) {	/* SDv2 */
                        //printf("SD version 2\n\r");   //debug printing
			for (n = 0; n < 4; n++) ocr[n] = read_spi();		/* Get trailing return value of R7 resp */
			if (ocr[2] == 0x01 && ocr[3] == 0xAA) {			/* The card can work at vdd range of 2.7-3.6V */
                                //printf("The card can work at vdd range of 2.7-3.6V\n\r"); //debug printing
				for (tmr = 10000; tmr && send_cmd(ACMD41, 1UL << 30); tmr--) dly_100us();	/* Wait for leaving idle state (ACMD41 with HCS bit) */
				if (tmr && send_cmd(CMD58, 0) == 0) {		/* Check CCS bit in the OCR */
					for (n = 0; n < 4; n++) ocr[n] = read_spi();
					ty = (ocr[0] & 0x40) ? CT_SD2 | CT_BLOCK : CT_SD2;	/* SDv2 (HC or SC) */
                                        //printf("HC or SC\n\r");   //debug printing
				}
			}
		} else {				/* SDv1 or MMCv3 */
                        //printf("SD version 1 or MMC version 3\n\r");  //debug printing
			if (send_cmd(ACMD41, 0) <= 1) 	{
                                //printf("SD version 1\n\r");   //debug printing
				ty = CT_SD1; cmd = ACMD41;	/* SDv1 */
			} else {
                                //printf("MMC version 3\n\r");  //debug printing
				ty = CT_MMC; cmd = CMD1;	/* MMCv3 */
			}
			for (tmr = 10000; tmr && send_cmd(cmd, 0); tmr--) dly_100us();	/* Wait for leaving idle state */
			if (!tmr || send_cmd(CMD16, 512) != 0)			/* Set R/W block length to 512 */
				ty = 0;
		}
	}
	CardType = ty;
	DESELECT();
	read_spi();

	return ty ? 0 : STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Read Partial Sector                                                   */
/*-----------------------------------------------------------------------*/
DRESULT disk_readp (
	BYTE* buff,		/* Pointer to the destination object */
	DWORD sector,	/* Sector number (LBA) */
	UINT offset,	/* Offset in the sector */
	UINT count		/* Byte count (bit15:destination) */
)
{
	DRESULT res;

	BYTE rc;
	UINT bc;


	if (!(CardType & CT_BLOCK)) sector *= 512;	/* Convert to byte address if needed */

	res = RES_ERROR;
	if (send_cmd(CMD17, sector) == 0) {		/* READ_SINGLE_BLOCK */

		bc = 40000;
		do {							/* Wait for data packet */
			rc = read_spi();
		} while (rc == 0xFF && --bc);

		if (rc == 0xFE) {				/* A data packet arrived */
			bc = 514 - offset - count;

			/* Skip leading bytes */
			if (offset) {
				do read_spi(); while (--offset);
			}

			/* Receive a part of the sector */
			if (buff) {	/* Store data to the memory */
				do {
					*buff++ = read_spi();
				} while (--count);
			}

			/* Skip remaining bytes and CRC */
			do read_spi(); while (--bc);

			res = RES_OK;
		}
	}

	DESELECT();
	read_spi();

	return res;
}


#ifdef _USE_WRITE
/*-----------------------------------------------------------------------*/
/* Write Partial Sector                                                  */
/*-----------------------------------------------------------------------*/

DRESULT disk_writep (
	BYTE* buff,		/* Pointer to the data to be written, NULL:Initiate/Finalize write operation */
	DWORD sc		/* Sector number (LBA) or Number of bytes to send */
)
{
	DRESULT res;


	if (!buff) {
		if (sc) {

			// Initiate write process

		} else {

			// Finalize write process

		}
	} else {

		// Send data to the disk

	}

	return res;
}
#endif
