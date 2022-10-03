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

#define SDCARD_CSD_MAP_A(XX)                                                                                           \
	XX(structure, Structure, 126, 2)                                                                                   \
	XX(taac, uint8_t, 112, 8)                                                                                          \
	XX(nsac, uint8_t, 104, 8)                                                                                          \
	XX(tran_speed, uint8_t, 96, 8)                                                                                     \
	XX(ccc, uint16_t, 84, 12)                                                                                          \
	XX(read_bl_len, uint8_t, 80, 4)                                                                                    \
	XX(read_bl_partial, uint8_t, 79, 1)                                                                                \
	XX(write_blk_missalign, bool, 80, 1)                                                                               \
	XX(read_blk_misalign, bool, 78, 1)                                                                                 \
	XX(dsr_imp, bool, 76, 1)

// Version 1.0
#define SDCARD_CSD_MAP_B1(XX)                                                                                          \
	XX(c_size, uint16_t, 62, 12)                                                                                       \
	XX(vdd_r_curr_min, uint8_t, 59, 3)                                                                                 \
	XX(vdd_r_curr_max, uint8_t, 56, 3)                                                                                 \
	XX(vdd_w_curr_min, uint8_t, 53, 3)                                                                                 \
	XX(vdd_w_curr_max, uint8_t, 50, 3)                                                                                 \
	XX(c_size_mult, uint8_t, 47, 3)

// Version 2.0
#define SDCARD_CSD_MAP_B2(XX) XX(c_size, uint32_t, 48, 22)

// Version 3.0
#define SDCARD_CSD_MAP_B3(XX) XX(c_size, uint32_t, 48, 28)

#define SDCARD_CSD_MAP_C(XX)                                                                                           \
	XX(erase_blk_en, bool, 46, 1)                                                                                      \
	XX(sector_size, uint8_t, 39, 7)                                                                                    \
	XX(wp_grp_size, uint8_t, 32, 7)                                                                                    \
	XX(wp_grp_enable, bool, 31, 1)                                                                                     \
	XX(r2w_factor, uint8_t, 26, 3)                                                                                     \
	XX(write_bl_len, uint8_t, 22, 4)                                                                                   \
	XX(write_bl_partial, bool, 21, 1)                                                                                  \
	XX(file_format_grp, bool, 15, 1)                                                                                   \
	XX(copy, bool, 14, 1)                                                                                              \
	XX(perm_write_protect, bool, 13, 1)                                                                                \
	XX(tmp_write_protect, bool, 12, 1)                                                                                 \
	XX(file_format, uint8_t, 10, 2)                                                                                    \
	XX(wp_upc, bool, 9, 1)                                                                                             \
	XX(crc, uint8_t, 1, 7)

namespace Storage
{
namespace SD
{
#define XX(tag, Type, start, len, ...)                                                                                 \
	Type tag() const                                                                                                   \
	{                                                                                                                  \
		return Type(readBits(start, len));                                                                             \
	}

struct CSD {
	uint32_t raw_bits[4];

	enum class Structure {
		v1 = 0, ///< SDC ver 1.XX or MMC
		v2 = 1, ///< SDC ver 2.XX
		v3 = 2, ///< SDC ver 3.XX
	};

	SDCARD_CSD_MAP_A(XX)
	SDCARD_CSD_MAP_C(XX)

	size_t printTo(Print& p) const;

protected:
	uint32_t readBits(uint8_t start, uint8_t size) const
	{
		const uint32_t mask = (size < 32 ? 1 << size : 0) - 1;
		const unsigned off = 3 - (start / 32);
		const unsigned shift = start & 31;
		uint32_t res = __builtin_bswap32(raw_bits[off]) >> shift;
		if(size + shift > 32) {
			res |= __builtin_bswap32(raw_bits[off - 1]) << ((32 - shift) % 32);
		}
		return res & mask;
	}
};

struct CSD1 : public CSD {
	SDCARD_CSD_MAP_B1(XX)

	uint64_t size() const
	{
		return uint64_t(c_size() + 1) << (c_size_mult() + 2) << read_bl_len();
	}
};

struct CSD2 : public CSD {
	SDCARD_CSD_MAP_B2(XX)

	uint64_t size() const
	{
		return uint64_t(c_size() + 1) * 512 * 1024;
	}
};

struct CSD3 : public CSD {
	SDCARD_CSD_MAP_B3(XX)

	uint64_t size() const
	{
		return uint64_t(c_size() + 1) * 512 * 1024;
	}
};

#undef XX

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

class Card : public Device
{
public:
	Card(SPIBase& spi) : Device(), spi(spi)
	{
	}

	/**
	 * @brief Initialise the card
	 * @param chipSelect
	 * @param freq SPI frequency in Hz, use 0 for maximum supported frequency
	 */
	bool begin(uint8_t chipSelect, uint32_t freq = 0);

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

	const CID& cid{mCID};
	const CSD& csd{mCSD};

private:
	static constexpr uint8_t sectorSizeBits{9}; // 512 bytes per sector

	bool wait_ready();
	void deselect();
	bool select();
	bool rcvr_datablock(void* buff, size_t btr);
	bool xmit_datablock(const void* buff, uint8_t token);
	uint8_t send_cmd(uint8_t cmd, uint32_t arg);
	bool read_cid();

	SPIBase& spi;
	CSD mCSD;
	CID mCID;
	uint64_t sectorCount{0};
	uint8_t chipSelect{255};
	bool initialised{false};
	uint8_t cardType; ///< b0:MMC, b1:SDv1, b2:SDv2, b3:Block addressing
};					  // namespace SD

} // namespace SD
} // namespace Storage

String toString(Storage::SD::CSD::Structure structure);
