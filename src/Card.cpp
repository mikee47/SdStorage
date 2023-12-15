/*
Modified by: (github.com/)ADiea
Project: Sming for ESP8266 - https://github.com/anakod/Sming
License: MIT
Date: 15.07.2015
Descr: Low-level SDCard functions
*/
/*------------------------------------------------------------------------/
/  Foolproof MMCv3/SDv1/SDv2 (in SPI mode) control module
/-------------------------------------------------------------------------/
/
/  Copyright (C) 2013, ChaN, all right reserved.
/
/ * This software is a free software and there is NO WARRANTY.
/ * No restriction on use. You can use, modify and redistribute it for
/   personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.
/ * Redistributions of source code must retain the above copyright notice.
/
/-------------------------------------------------------------------------/
  Features and Limitations:

  * Easy to Port Bit-banging SPI
    It uses only four GPIO pins. No complex peripheral needs to be used.

  * Platform Independent
    You need to modify only a few macros to control the GPIO port.

  * Low Speed
    The data transfer rate will be several times slower than hardware SPI.

  * No Media Change Detection
    Application program needs to perform a f_mount() after media change.

/-------------------------------------------------------------------------*/

#include "include/Storage/SD/Card.h"
#include <Storage/Disk.h>
#include <Clock.h>
#include <debug_progmem.h>

/* MMC/SD command (SPI mode) */
enum Command : uint8_t {
	CMD0 = 0,			// GO_IDLE_STATE
	CMD1 = 1,			// SEND_OP_COND
	ACMD41 = 0x80 | 41, // SEND_OP_COND (SDC)
	CMD8 = 8,			// SEND_IF_COND
	CMD9 = 9,			// SEND_CSD
	CMD10 = 10,			// SEND_CID
	CMD12 = 12,			// STOP_TRANSMISSION
	CMD13 = 13,			// SEND_STATUS
	ACMD13 = 0x80 | 13, // SD_STATUS (SDC)
	CMD16 = 16,			// SET_BLOCKLEN
	CMD17 = 17,			// READ_SINGLE_BLOCK
	CMD18 = 18,			// READ_MULTIPLE_BLOCK
	CMD23 = 23,			// SET_BLOCK_COUNT
	ACMD23 = 0x80 | 23, // SET_WR_BLK_ERASE_COUNT (SDC)
	CMD24 = 24,			// WRITE_BLOCK
	CMD25 = 25,			// WRITE_MULTIPLE_BLOCK
	CMD32 = 32,			// ERASE_ER_BLK_START
	CMD33 = 33,			// ERASE_ER_BLK_END
	CMD38 = 38,			// ERASE
	CMD55 = 55,			// APP_CMD
	CMD58 = 58,			// READ_OCR
};

/* MMC card type flags (MMC_GET_TYPE) */
enum CardType {
	CT_MMC = 0x01,			  // MMC ver 3
	CT_SD1 = 0x02,			  // SD ver 1
	CT_SD2 = 0x04,			  // SD ver 2
	CT_SDC = CT_SD1 | CT_SD2, // SD
	CT_BLOCK = 0x08,		  // Block addressing
};

// Data block transfer control tokens
enum Token {
	TK_START_BLOCK_SINGLE = 0xfe,
	TK_START_BLOCK_MULTI = 0xfc,
	TK_STOP_TRAN = 0xfd,
};

#define CHECK_INIT()                                                                                                   \
	if(!initialised) {                                                                                                 \
		return false;                                                                                                  \
	}

namespace Storage::SD
{
/*
 * Wait for card ready
 */
bool Card::wait_ready() /* 1:OK, 0:Timeout */
{
	for(unsigned tmr = 5000; tmr; tmr--) { /* Wait for ready in timeout of 500ms */
		HSPI::Request req;
		req.out.set8(0xff);
		req.in.set8(0);
		spi.execute(req);
		if(req.in.data8 == 0xff) {
			return true;
		}
		delayMicroseconds(100);
	}

	return false;
}

/*
 * Deselect the card and release SPI bus
 */
void Card::deselect()
{
	// digitalWrite(chipSelect, HIGH);
	// spi.transfer(0xff); /* Send 0xFF Dummy clock (force DO hi-z for multiple slave SPI) */
}

/**
 * Select the card and wait for ready
 *
 * Returns true: OK, false: Timeout
 */
bool Card::select()
{
	// digitalWrite(chipSelect, LOW);
	/* Dummy clock (force DO enabled) */
	HSPI::Request req;
	req.out.set8(0xff);
	spi.execute(req);
	if(wait_ready()) {
		return true;
	}

	debug_e("[SD] select() failed");
	deselect();
	return false;
}

/*
 * Receive a data packet from the card
 */
bool Card::rcvr_datablock(void* buff, size_t btr)
{
	/* Wait for data packet in timeout of 100ms */
	HSPI::Request req;
	req.out.set8(0xff);
	req.in.set8(0xff);
	for(unsigned tmr = 1000; tmr; tmr--) {
		spi.execute(req);
		if(req.in.data8 != 0xFF) {
			break;
		}
		delayMicroseconds(100);
	}
	if(req.in.data8 != 0xFE) {
		return false; /* If not valid data token, return with error */
	}

	memset(buff, 0xFF, btr);
	req.out.set(buff, btr);
	req.in.set(buff, btr);
	spi.execute(req);

	// keep MOSI HIGH, discard CRC
	req.out.set16(0xffff);
	req.in.clear();
	spi.execute(req);

	// success
	return true;
}

/*
 * Send a data packet to the card
 */
bool Card::xmit_datablock(const void* buff, uint8_t token)
{
	if(!wait_ready()) {
		debug_e("[SD] wait_ready failed");
		return false;
	}

	// Send the token
	HSPI::Request req;
	req.out.set8(token);
	spi.execute(req);
	if(token == TK_STOP_TRAN) {
		return true;
	}

	// Data
	req.out.set(buff, sectorSize);
	spi.execute(req);
	// Dummy CRC
	req.out.set16(0xffff);
	spi.execute(req);

	// Keep MOSI HIGH, read response
	req.out.set8(0xff);
	req.in.set8(0);
	spi.execute(req);

	// If not accepted, return with error
	if((req.in.data8 & 0x1F) != 0x05) {
		debug_e("[SDCard] data not accepted, d = 0x%02x", req.in.data8);
		return false;
	}

	return true;
}

/*
 * Send a command packet to the card
 *
 * Returns Command response (bit7: Send failed)
 */
uint8_t Card::send_cmd(uint8_t cmd, uint32_t arg)
{
	if(cmd & 0x80) { /* ACMD<n> is the command sequence of CMD55-CMD<n> */
		cmd &= 0x7F;
		uint8_t n = send_cmd(CMD55, 0);
		if(n > 1) {
			debug_e("[SD] CMD55 error, n = 0x%02x", n);
			return n;
		}
	}

	/* Select the card and wait for ready except to stop multiple block read */
	if(cmd != CMD12) {
		deselect();
		if(!select()) {
			debug_e("[SD] Select failed");
			return 0xFF;
		}
	}

	/* Send a command packet */
	uint8_t crc;
	if(cmd == CMD0) {
		crc = 0x95; // CRC for CMD0(0)
	} else if(cmd == CMD8) {
		crc = 0x87; // CRC for CMD8(0x1AA)
	} else {
		crc = 0x01; // Dummy CRC + Stop
	}

	uint8_t buf[]{
		uint8_t(0x40 | cmd), // Start + Command index
		uint8_t(arg >> 24),  // Argument[31..24]
		uint8_t(arg >> 16),  // Argument[23..16]
		uint8_t(arg >> 8),   // Argument[15..8]
		uint8_t(arg),		 // Argument[7..0]
		crc,
		0xff, // Dummy clock (force DO enabled)
		// Response
		0xff,
		0xff,
	};
	unsigned len = (cmd == CMD12) ? sizeof(buf) : (sizeof(buf) - 1);
	HSPI::Request req;
	req.out.set(buf, len);
	req.in.set(buf, len);

	debug_hex(DBG, "SPI > ", buf, len);
	spi.execute(req);
	debug_hex(DBG, "SPI < ", buf, len);

	auto d = buf[len - 1];

	debug_d("[SD] send_cmd(%u): 0x%02x", cmd, d);
	return d;
}

bool Card::begin(HSPI::PinSet pinSet, uint8_t chipSelect, uint32_t freq)
{
	if(initialised) {
		return false;
	}

	const uint32_t maxFreq{40000000U};
	if(freq == 0 || freq > maxFreq) {
		freq = maxFreq;
	}
	if(!spi.begin(pinSet, chipSelect, freq)) {
		debug_e("[SD] SPI init failed");
		return false;
	}

	spi.setBitOrder(MSBFIRST);
	spi.setClockMode(HSPI::ClockMode::mode0);
	spi.setIoMode(HSPI::IoMode::SPI);

	delayMicroseconds(10000);

	cardType = init();

	if(cardType == 0) {
		debug_e("[SD] init FAIL");
	} else {
		initialised = true;
		debug_i("[SD] OK: TYPE %u", cardType);
	}

	deselect();

	if(initialised) {
		Disk::scanPartitions(*this);
	}

	return initialised;
}

void Card::end()
{
	spi.end();
	initialised = false;
}

uint8_t Card::init()
{
	// init send 0xFF x 80
	uint8_t tmp[80 / 8];
	memset(tmp, 0xff, sizeof(tmp));
	HSPI::Request req;
	req.out.set(tmp, sizeof(tmp));
	spi.execute(req);

	//!! OK to here

	// send n send_cmd(CMD0, 0)");
	uint8_t retCmd;
	uint8_t n = 5;
	do {
		retCmd = send_cmd(CMD0, 0);
		n--;
	} while(n && retCmd != 1);

	if(retCmd != 1) {
		debug_e("[SD] ERROR: %x", retCmd);
		return 0;
	}

	uint8_t ty = 0;

	// Enter Idle state
	// if(send_cmd(CMD8, 0x1AA) == 1) { /* SDv2? */
	uint8_t buf[]{
		uint8_t(0x40 | CMD8), // Start + Command index
		0,
		0,
		0x01,
		0xAA,
		0x87, // crc
		0xff, // Dummy clock (force DO enabled)
		0xff, // Response
		// Result
		0xff,
		0xff,
		0xff,
		0xff,
	};
	req.out.set(buf, sizeof(buf));
	req.in.set(buf, sizeof(buf));
	spi.execute(req);

	if(buf[7] == 0x01) {
		debug_i("[SD] Sdv2 ?");
		const uint8_t* res = &buf[8];
		debug_hex(INFO, "[SD] IF COND", res, 4);

		// Check card can work at vdd range of 2.7-3.6V
		if(res[2] != 0x01 || res[3] != 0xAA) {
			debug_e("[SD] VDD invalid");
			return 0;
		}

		// Wait for leaving idle state (ACMD41 with HCS bit)
		unsigned tmr;
		for(tmr = 1000; tmr; tmr--) {
			if(send_cmd(ACMD41, 1UL << 30) == 0) {
				debug_d("[SD] ACMD41 OK");
				break;
			}
			delayMicroseconds(1000);
		}
		if(tmr == 0) {
			debug_e("[SD] ACMD41 FAIL");
			return 0;
		}

		// Check CCS bit in the OCR
		if(send_cmd(CMD58, 0) != 0) {
			debug_e("[SD] OCR read failed");
			return 0;
		}

		req.out.set32(0xffffffff);
		req.in.set32(0);
		spi.execute(req);
		ty = (req.in.data[0] & 0x40) ? CT_SD2 | CT_BLOCK : CT_SD2; /* SDv2 */
		debug_hex(INFO, "[SD] OCR", req.in.data, 4);
	} else { /* SDv1 or MMCv3 */
		debug_i("[SD] Sdv1 / MMCv3 ?");
		uint8_t cmd;
		if(send_cmd(ACMD41, 0) <= 1) {
			ty = CT_SD1;
			cmd = ACMD41; /* SDv1 */
		} else {
			ty = CT_MMC;
			cmd = CMD1; /* MMCv3 */
		}
		unsigned tmr;
		for(tmr = 1000; tmr; tmr--) { /* Wait for leaving idle state */
			if(send_cmd(cmd, 0) == 0) {
				break;
			}
			delayMicroseconds(1000);
		}
		if(tmr == 0) {
			debug_i("[SD] tmr = 0");
			return 0;
		}
		/* Set R/W block length to 512 */
		if(send_cmd(CMD16, sectorSize) != 0) {
			debug_i("[SD] CMD16 != 0");
			return 0;
		}
	}

	// Get number of sectors on the disk
	if(send_cmd(CMD9, 0) != 0 || !rcvr_datablock(&mCSD, sizeof(mCSD))) {
		debug_e("[SD] Read CSD failed");
		return 0;
	}
	mCSD.bswap();

	uint64_t size = mCSD.getSize();
#ifndef ENABLE_STORAGE_SIZE64
	if(isSize64(size)) {
		debug_e("[SD] Device size %llu requires ENABLE_STORAGE_SIZE64=1", size);
		return 0;
	}
#endif
	sectorCount = size >> sectorSizeShift;
	if(sectorCount == 0) {
		debug_e("[SD] Size invalid %llu", size);
		return 0;
	}

	if(send_cmd(CMD10, 0) != 0 || !rcvr_datablock(&mCID, sizeof(mCID))) {
		debug_e("[SD] Read CID failed");
		return 0;
	}
	mCID.bswap();

	return ty;
} // namespace Storage::SD

bool Card::raw_sector_read(storage_size_t address, void* dst, size_t size)
{
	CHECK_INIT()

	// Convert byte address to sector number for block devices
	if((cardType & CT_BLOCK) == 0) {
		address <<= sectorSizeShift;
	}

	uint8_t cmd = (size > 1) ? CMD18 : CMD17; /*  READ_MULTIPLE_BLOCK : READ_SINGLE_BLOCK */
	if(send_cmd(cmd, address) == 0) {
		for(auto bufptr = static_cast<uint8_t*>(dst); size != 0; --size, bufptr += sectorSize) {
			if(!rcvr_datablock(bufptr, sectorSize)) {
				debug_e("[SD] rcvr error");
				break;
			}
		}
		if(cmd == CMD18) {
			send_cmd(CMD12, 0); /* STOP_TRANSMISSION */
		}
	}
	deselect();

	return size == 0;
}

bool Card::raw_sector_write(storage_size_t address, const void* src, size_t size)
{
	CHECK_INIT()

	// If required, convert sector address to byte offset
	if((cardType & CT_BLOCK) == 0) {
		address <<= sectorSizeShift;
	}

	if(size == 1) {
		// Single block write
		if((send_cmd(CMD24, address) == 0) && xmit_datablock(src, TK_START_BLOCK_SINGLE)) {
			size = 0;
		} else {
			debug_e("[SD] CMD24 error");
		}
	} else {
		// Multiple block write
		if(cardType & CT_SDC) {
			// SET_WR_BLK_ERASE_COUNT
			send_cmd(ACMD23, size);
		}
		//  WRITE_MULTIPLE_BLOCK
		if(send_cmd(CMD25, address) == 0) {
			for(auto bufptr = static_cast<const uint8_t*>(src); size != 0; --size, bufptr += sectorSize) {
				if(!xmit_datablock(bufptr, TK_START_BLOCK_MULTI)) {
					debug_e("[SD] xmit error");
					break;
				}
			}

			if(!xmit_datablock(0, TK_STOP_TRAN)) {
				debug_e("[SD] STOP_TRAN error");
				size = 1;
			}
		}
	}
	deselect();

	return size == 0;
}

bool Card::raw_sector_erase_range(storage_size_t address, size_t size)
{
	CHECK_INIT()

	if((cardType & CT_BLOCK) == 0) {
		address <<= sectorSizeShift;
		size <<= sectorSizeShift;
	}

	// ERASE_WR_BLK_START, ERASE_WR_BLK_END, ERASE / DISCARD
	bool res =
		send_cmd(CMD32, address) == 0 && send_cmd(CMD33, address + size - 1) == 0 && send_cmd(CMD38, 0x00000001) == 0;

	deselect();

	return res;
}

bool Card::raw_sync()
{
	if(!initialised) {
		return false;
	}

	// Make sure that no pending write process
	bool res = select();
	deselect();
	return res;
}

} // namespace Storage::SD
