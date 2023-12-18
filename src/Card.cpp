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
	CMD0 = 0,			// R1 GO_IDLE_STATE
	CMD1 = 1,			// R1 SEND_OP_COND
	ACMD41 = 0x80 | 41, // R1 SEND_OP_COND (SDC)
	CMD8 = 8,			// R7 SEND_IF_COND
	CMD9 = 9,			// R1 SEND_CSD
	CMD10 = 10,			// R1 SEND_CID
	CMD12 = 12,			// R1b STOP_TRANSMISSION
	CMD13 = 13,			// R2 SEND_STATUS
	ACMD13 = 0x80 | 13, // R2 SD_STATUS (SDC)
	CMD16 = 16,			// R1 SET_BLOCKLEN
	CMD17 = 17,			// R1 READ_SINGLE_BLOCK
	CMD18 = 18,			// R1 READ_MULTIPLE_BLOCK
	CMD23 = 23,			// ?  SET_BLOCK_COUNT
	ACMD23 = 0x80 | 23, // R1 SET_WR_BLK_ERASE_COUNT (SDC)
	CMD24 = 24,			// R1 WRITE_BLOCK
	CMD25 = 25,			// R1 WRITE_MULTIPLE_BLOCK
	CMD32 = 32,			// R1 ERASE_ER_BLK_START
	CMD33 = 33,			// R1 ERASE_ER_BLK_END
	CMD38 = 38,			// R1b ERASE
	CMD55 = 55,			// R1 APP_CMD
	CMD58 = 58,			// R3 READ_OCR
};

/* Responses

	Type	Length (bytes)
	R1		1
	R1b		1 with busy
	R2		2
	R3		5 R1 + 4 bytes
	R7		5 R1 + 4 bytes

 */

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
uint8_t crc7(const void* data, size_t len)
{
	const uint8_t crc7_table[256]{
		0x00, 0x09, 0x12, 0x1b, 0x24, 0x2d, 0x36, 0x3f, 0x48, 0x41, 0x5a, 0x53, 0x6c, 0x65, 0x7e, 0x77, 0x19, 0x10,
		0x0b, 0x02, 0x3d, 0x34, 0x2f, 0x26, 0x51, 0x58, 0x43, 0x4a, 0x75, 0x7c, 0x67, 0x6e, 0x32, 0x3b, 0x20, 0x29,
		0x16, 0x1f, 0x04, 0x0d, 0x7a, 0x73, 0x68, 0x61, 0x5e, 0x57, 0x4c, 0x45, 0x2b, 0x22, 0x39, 0x30, 0x0f, 0x06,
		0x1d, 0x14, 0x63, 0x6a, 0x71, 0x78, 0x47, 0x4e, 0x55, 0x5c, 0x64, 0x6d, 0x76, 0x7f, 0x40, 0x49, 0x52, 0x5b,
		0x2c, 0x25, 0x3e, 0x37, 0x08, 0x01, 0x1a, 0x13, 0x7d, 0x74, 0x6f, 0x66, 0x59, 0x50, 0x4b, 0x42, 0x35, 0x3c,
		0x27, 0x2e, 0x11, 0x18, 0x03, 0x0a, 0x56, 0x5f, 0x44, 0x4d, 0x72, 0x7b, 0x60, 0x69, 0x1e, 0x17, 0x0c, 0x05,
		0x3a, 0x33, 0x28, 0x21, 0x4f, 0x46, 0x5d, 0x54, 0x6b, 0x62, 0x79, 0x70, 0x07, 0x0e, 0x15, 0x1c, 0x23, 0x2a,
		0x31, 0x38, 0x41, 0x48, 0x53, 0x5a, 0x65, 0x6c, 0x77, 0x7e, 0x09, 0x00, 0x1b, 0x12, 0x2d, 0x24, 0x3f, 0x36,
		0x58, 0x51, 0x4a, 0x43, 0x7c, 0x75, 0x6e, 0x67, 0x10, 0x19, 0x02, 0x0b, 0x34, 0x3d, 0x26, 0x2f, 0x73, 0x7a,
		0x61, 0x68, 0x57, 0x5e, 0x45, 0x4c, 0x3b, 0x32, 0x29, 0x20, 0x1f, 0x16, 0x0d, 0x04, 0x6a, 0x63, 0x78, 0x71,
		0x4e, 0x47, 0x5c, 0x55, 0x22, 0x2b, 0x30, 0x39, 0x06, 0x0f, 0x14, 0x1d, 0x25, 0x2c, 0x37, 0x3e, 0x01, 0x08,
		0x13, 0x1a, 0x6d, 0x64, 0x7f, 0x76, 0x49, 0x40, 0x5b, 0x52, 0x3c, 0x35, 0x2e, 0x27, 0x18, 0x11, 0x0a, 0x03,
		0x74, 0x7d, 0x66, 0x6f, 0x50, 0x59, 0x42, 0x4b, 0x17, 0x1e, 0x05, 0x0c, 0x33, 0x3a, 0x21, 0x28, 0x5f, 0x56,
		0x4d, 0x44, 0x7b, 0x72, 0x69, 0x60, 0x0e, 0x07, 0x1c, 0x15, 0x2a, 0x23, 0x38, 0x31, 0x46, 0x4f, 0x54, 0x5d,
		0x62, 0x6b, 0x70, 0x79,
	};

	uint8_t crc{0};
	auto ptr = static_cast<const uint8_t*>(data);
	while(len--) {
		crc = crc7_table[(crc << 1) ^ *ptr++];
	}
	return crc;
}

uint16_t crc16(uint16_t crc, const void* data, size_t len)
{
	const uint16_t table[256]{
		0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7, 0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad,
		0xe1ce, 0xf1ef, 0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6, 0x9339, 0x8318, 0xb37b, 0xa35a,
		0xd3bd, 0xc39c, 0xf3ff, 0xe3de, 0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485, 0xa56a, 0xb54b,
		0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d, 0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
		0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc, 0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861,
		0x2802, 0x3823, 0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b, 0x5af5, 0x4ad4, 0x7ab7, 0x6a96,
		0x1a71, 0x0a50, 0x3a33, 0x2a12, 0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a, 0x6ca6, 0x7c87,
		0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41, 0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
		0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70, 0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a,
		0x9f59, 0x8f78, 0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f, 0x1080, 0x00a1, 0x30c2, 0x20e3,
		0x5004, 0x4025, 0x7046, 0x6067, 0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e, 0x02b1, 0x1290,
		0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256, 0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
		0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405, 0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e,
		0xc71d, 0xd73c, 0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634, 0xd94c, 0xc96d, 0xf90e, 0xe92f,
		0x99c8, 0x89e9, 0xb98a, 0xa9ab, 0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3, 0xcb7d, 0xdb5c,
		0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a, 0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
		0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9, 0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83,
		0x1ce0, 0x0cc1, 0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8, 0x6e17, 0x7e36, 0x4e55, 0x5e74,
		0x2e93, 0x3eb2, 0x0ed1, 0x1ef0,
	};

	auto ptr = static_cast<const uint8_t*>(data);
	while(len--) {
		crc = table[(crc >> 8) ^ *ptr++] ^ (crc << 8);
	}
	return crc;
}

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
	if(req.in.data8 != TK_START_BLOCK_SINGLE) {
		debug_e("SD RCV 0x%02x", req.in.data8);
		return false; /* If not valid data token, return with error */
	}

	memset(buff, 0xFF, btr);
	req.out.set(buff, btr);
	req.in.set(buff, btr);
	spi.execute(req);

	// keep MOSI HIGH, discard CRC
	req.out.set16(0xffff);
	req.in.set16(0);
	spi.execute(req);

	debug_hex(DBG, "SD RCV", buff, btr);

	// Validate CRC
	uint16_t crcRx = (req.in.data[0] << 8) | req.in.data[1];
	uint16_t crcCalc = crc16(0, buff, btr);
	debug_d("SD CRC 0x%04x, rx 0x%04x", crcCalc, crcRx);
	return crcRx == crcCalc;
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
	auto rsp = req.in.data8;
	if((rsp & 0x1F) != 0x05) {
		debug_e("[SD] data not accepted, d = 0x%02x", rsp);
		return false;
	}

	return true;
}

/*
 * Send a command packet to the card
 *
 * Returns Command response (bit7: Send failed)
 */
uint8_t Card::send_cmd(uint8_t cmd, uint32_t arg, void* response, size_t responseSize)
{
	if(cmd & 0x80) { /* ACMD<n> is the command sequence of CMD55-CMD<n> */
		cmd &= 0x7F;
		uint8_t n = send_cmd(CMD55, 0);
		if(n > 1) {
			debug_e("[SD] CMD55 error, n = 0x%02x", n);
			return n;
		}
	}

	/*
		CID/CSD responses and READ commands have an additional stuffing/delay byte at the start,
		i.e. ff 01 instead of 01.
		This is followed by ff fe (for start of data block) then 16 bytes then 2 byte CRC.
		If this isn't done in the same transaction then bit 7 of the first response byte gets
		set which corrupts the data.
		We'll therefore need a larger buffer, best to allocate that as a class member.

		maxWaitBytes is going to be partly dependent on clock speed
	*/
	Buffer buffer;
	memset(&buffer, 0xff, sizeof(buffer));
	buffer.cmd = 0x40 | cmd;
	buffer.arg = HSPI::bswap32(arg);
	auto crc = crc7(&buffer.cmd, 5);
	buffer.crcAndStop = (crc << 1) | 0x01;
	unsigned len = offsetof(Buffer, data) + 1;
	if(cmd == CMD8 || cmd == CMD58) {
		len += 4;
	} else if(cmd == CMD13) {
		len += 1 + 1 + maxWaitBytes + 64 + 2;
	} else if(cmd == CMD9 || cmd == CMD10) {
		len += 1 + 2 + 16 + 2;
	} else if(cmd == CMD17) {
		len += 1 + 2 + maxWaitBytes + 512 + 2;
	}
	HSPI::Request req;
	req.out.set(&buffer, len);
	req.in.set(&buffer, len);

	debug_hex(DBG, "SPI > ", &buffer, len);
	spi.execute(req);
	debug_hex(DBG, "SPI < ", &buffer, len);

	unsigned i = 0;
	uint8_t d = buffer.data[i++];
	if(cmd == CMD9 || cmd == CMD10 || cmd == CMD17 || cmd == CMD13) {
		if(d & 0x80) {
			d = buffer.data[i++];
		}
		if(cmd == CMD13) {
			++i; // 2 status bytes
		}
		while(i < (2 + maxWaitBytes) && buffer.data[i] == 0xff) {
			++i;
		}
		++i; // Skip 0xfe

		// Validate CRC
		uint16_t crcRx = (buffer.data[i + responseSize] << 8) | buffer.data[i + responseSize + 1];
		uint16_t crcCalc = crc16(0, &buffer.data[i], responseSize);
		debug_d("SD CRC 0x%04x, rx 0x%04x", crcCalc, crcRx);
		if(crcCalc != crcRx) {
			d = 0xff;
		}
	}
	if(response) {
		memcpy(response, &buffer.data[i], responseSize);
	}

	debug_d("[SD] send_cmd(%u): 0x%02x", cmd, d);
	return d;
}

bool Card::send_cmd_with_retry(uint8_t cmd, uint32_t arg, uint8_t requiredResponse, unsigned maxAttempts)
{
	while(maxAttempts--) {
		if(send_cmd(cmd, arg) == requiredResponse) {
			return true;
		}
		delayMicroseconds(1000);
	}
	return false;
}

bool Card::begin(HSPI::PinSet pinSet, uint8_t chipSelect, uint32_t freq)
{
	if(initialised) {
		return false;
	}

	// Require low speed for initialisation
	if(!spi.begin(pinSet, chipSelect, 400000)) {
		debug_e("[SD] SPI init failed");
		return false;
	}

	spi.setBitOrder(MSBFIRST);
	spi.setClockMode(HSPI::ClockMode::mode0);
	spi.setIoMode(HSPI::IoMode::SPI);

	cardType = init();

	if(cardType == 0) {
		debug_e("[SD] init FAIL");
		spi.end();
		return false;
	}

	// Adjust clock speed for normal operation
	const uint32_t maxFreq{40000000U};
	if(freq == 0 || freq > maxFreq) {
		freq = maxFreq;
	}
	spi.setClockSpeed(freq);

	initialised = true;
	debug_d("[SD] OK: TYPE %u", cardType);

	Disk::scanPartitions(*this);

	return true;
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
	req.chipsel = 0; // Keep CS HIGH
	spi.execute(req);
	req.chipsel = 1;

	if(!send_cmd_with_retry(CMD0, 0, 0x01, 5)) {
		debug_e("[SD] ERROR CMD0");
		return 0;
	}

	wait_ready();

	uint8_t ty = 0;

	uint32_t ifcond;
	if(send_cmd(CMD8, 0x1AA, &ifcond, sizeof(ifcond)) == 0x01) { /* SDv2? */
		debug_d("[SD] Sdv2 ?");
		ifcond = HSPI::bswap32(ifcond);
		debug_d("[SD] IF COND 0x%08x", ifcond);

		// Check card can work at vdd range of 2.7-3.6V
		if((ifcond & 0xffff) != 0x01aa) {
			debug_e("[SD] VDD invalid");
			return 0;
		}

		// Wait for leaving idle state (ACMD41 with HCS bit)
		if(!send_cmd_with_retry(ACMD41, 1UL << 30, 0, 1000)) {
			debug_e("[SD] ACMD41 FAIL");
			return 0;
		}
		debug_d("[SD] ACMD41 OK");

		// Check CCS bit in the OCR
		uint32_t ocr;
		if(send_cmd(CMD58, 0, &ocr, sizeof(ocr)) != 0) {
			debug_e("[SD] OCR read failed");
			return 0;
		}
		ocr = HSPI::bswap32(ocr);
		ty = (ocr & 0x40000000) ? CT_SD2 | CT_BLOCK : CT_SD2; /* SDv2 */
		debug_d("[SD] OCR 0x%08x", ocr);
	} else { /* SDv1 or MMCv3 */
		wait_ready();
		debug_i("[SD] Sdv1 / MMCv3 ?");
		uint8_t cmd;
		if(send_cmd(ACMD41, 0) <= 1) {
			ty = CT_SD1;
			cmd = ACMD41; /* SDv1 */
		} else {
			ty = CT_MMC;
			cmd = CMD1; /* MMCv3 */
		}
		if(!send_cmd_with_retry(cmd, 0, 0, 1000)) {
			debug_i("[SD] cmd %u fail", cmd);
			return 0;
		}
		/* Set R/W block length to 512 */
		if(send_cmd(CMD16, sectorSize) != 0) {
			debug_i("[SD] CMD16 != 0");
			return 0;
		}
	}

	if(send_cmd(CMD9, 0, &mCSD, sizeof(mCSD)) != 0) {
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

	if(send_cmd(CMD10, 0, &mCID, sizeof(mCID)) != 0) {
		debug_e("[SD] Read CID failed");
		return 0;
	}
	mCID.bswap();

	uint8_t status[64];
	send_cmd(ACMD13, 0, status, sizeof(status));

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
	if(send_cmd(cmd, address, dst, sectorSize) == 0) {
		--size;
		// for(auto bufptr = static_cast<uint8_t*>(dst); size != 0; --size, bufptr += sectorSize) {
		// 	if(!rcvr_datablock(bufptr, sectorSize)) {
		// 		debug_e("[SD] rcvr error");
		// 		break;
		// 	}
		// }
		// if(cmd == CMD18) {
		// 	send_cmd(CMD12, 0); /* STOP_TRANSMISSION */
		// }
	}

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

	return res;
}

bool Card::raw_sync()
{
	return true;
}

} // namespace Storage::SD
