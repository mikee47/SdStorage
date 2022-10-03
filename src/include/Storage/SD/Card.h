/*
Author: (github.com/)ADiea
Project: Sming for ESP8266 - https://github.com/anakod/Sming
License: MIT
Date: 15.07.2015
Descr: Low-level SDCard functions
*/
#pragma once

#include <Storage/Device.h>
#include <SPIBase.h>

namespace Storage
{
namespace SD
{
class Card : public Device
{
public:
	// Version 1.0
	struct __attribute__((packed)) CSD {
		uint8_t structure : 2; // 0: Version 1.0, 1: Version 2.0, 2: Version 3.0, 3: reserved
		uint8_t reserved : 6;

		uint8_t taac;
		uint8_t nsac;
		uint8_t tran_speed;

		ccc : 12;
		read_bl_len : 4;

		read_bl_partial : 1;
		write_blk_missalign : 1;
		read_blk_misalign : 1;
		dsr_imp : 1;

		/////////
		// Version 1.0
		reserved : 2;
		c_size : 12;
		vdd_r_curr_min : 3;
		vdd_r_curr_max : 3;
		vdd_w_curr_min : 3;
		vdd_w_curr_max : 3;
		c_size_mult : 3;
		// Version 2.0
		reserved : 6;
		c_size : 22;
		reserved : 1;
		// Version 3.0
		c_size : 28;
		reserved : 1;
		/////////

		erase_blk_en : 1;
		sector_size : 7;
		wp_grp_size : 7;
		wp_grp_enable : 1;
		reserved : 2;

		r2w_factor : 3;
		write_bl_len : 4;
		write_bl_partial : 1;
		reserved : 5;

		file_format_grp : 1;
		copy : 1;
		perm_write_protect : 1;
		tmp_write_protect : 1;
		file_format : 2;
		wp_upc : 1;
		reserved : 1;
		crc : 7;
		not_used : 1;
	};

	/*
		Samsung 32GB EVO Plus CID: 1b 53 4d 45 42 31 51 54 30 f1 77 5f ea 01 1a b9  .SMEB1QT0.w_....

		1b                  MID
		53 4d	            OID: "SM"
		45 42 31 51 54 30   PNM: "EB1QT"
		30                  PRV
		f1 77 5f ea         PSN
		1a 01               MDT    0000 0001 0001 1010
		b9                  CRC7: 0x5C
	*/
	struct __attribute__((packed)) CID {
		uint8_t mid;		  ///< Manufacturer ID
		char oid[2];		  ///< OEM / Application ID
		char pnm[5];		  ///< Product name
		uint8_t prv;		  ///< Product revision
		uint32_t psn;		  ///< Product serial number
		uint16_t mdt;		  ///< Manufacturing date
		uint8_t not_used : 1; ///< Always 1
		uint8_t crc7 : 7;	 ///< 7-bit checksum

		void bswap()
		{
			psn = __builtin_bswap32(psn);
			mdt = __builtin_bswap16(mdt);
		}

		uint16_t mdt_year() const
		{
			return 2000 + ((mdt >> 4) & 0xff);
		}

		uint8_t mdt_month() const
		{
			return mdt & 0x0f;
		}

		uint8_t major() const
		{
			return prv >> 4;
		}

		uint8_t minor() const
		{
			return prv & 0x0f;
		}

		size_t printTo(Print& p) const;
	};
	static_assert(sizeof(CID) == 16, "Bad CID struct");

	Card(SPIBase& spi) : Device(), spi(spi)
	{
	}

	/**
	 * @brief Initialise the card
	 * @param chipSelect
	 * @param freq SPI frequency in Hz, use 0 for maximum supported frequency
	 */
	bool begin(uint8_t chipSelect, uint32_t freq = 0);

	bool read_cid(CID& cid);

	/* Storage Device methods */

	String getName() const override
	{
		return F("SDCard");
	}

	uint32_t getId() const
	{
		return 0;
	}

	Type getType() const
	{
		return Type::sdcard;
	}

	bool read(storage_size_t address, void* dst, size_t size) override;

	bool write(storage_size_t address, const void* src, size_t size) override;

	bool erase_range(storage_size_t address, size_t size) override
	{
		return false;
	}

	bool sync() override;

	size_t getBlockSize() const override
	{
		return 1 << sectorSizeBits;
	}

	storage_size_t getSize() const override
	{
		return storage_size_t(sectorCount) << sectorSizeBits;
	}

	/**
	 * @brief Get erase block size in sectors
	 */
	uint32_t getEraseBlockSize() const
	{
		return 128;
	}

private:
	static constexpr uint8_t sectorSizeBits{9}; // 512 bytes per sector

	bool wait_ready();
	void deselect();
	bool select();
	bool rcvr_datablock(void* buff, size_t btr);
	bool xmit_datablock(const void* buff, uint8_t token);
	uint8_t send_cmd(uint8_t cmd, uint32_t arg);

	SPIBase& spi;
	uint32_t sectorCount{0};
	uint8_t chipSelect{255};
	bool initialised{false};
	uint8_t cardType; ///< b0:MMC, b1:SDv1, b2:SDv2, b3:Block addressing
};

} // namespace SD
} // namespace Storage
