#pragma once

#include <Print.h>

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

namespace Storage::SD
{
#define XX(tag, Type, start, len, ...)                                                                                 \
	Type tag() const                                                                                                   \
	{                                                                                                                  \
		return Type(readBits(start, len));                                                                             \
	}

struct CSD1;
struct CSD2;
struct CSD3;

struct CSD {
	uint32_t raw_bits[4];

	enum class Structure {
		v1 = 0, ///< SDC ver 1.XX or MMC
		v2 = 1, ///< SDC ver 2.XX
		v3 = 2, ///< SDC ver 3.XX
	};

	void bswap()
	{
		for(auto& w : raw_bits) {
			w = __builtin_bswap32(w);
		}
	}

	SDCARD_CSD_MAP_A(XX)

	template <class C> const C& as() const
	{
		return *static_cast<const C*>(this);
	}

	const CSD1& v1() const
	{
		return as<CSD1>();
	}

	const CSD2& v2() const
	{
		return as<CSD2>();
	}

	const CSD3& v3() const
	{
		return as<CSD3>();
	}

	uint64_t getSize() const;

	SDCARD_CSD_MAP_C(XX)

	size_t printTo(Print& p) const;

protected:
	uint32_t readBits(uint8_t start, uint8_t size) const
	{
		const uint32_t mask = (size < 32 ? 1 << size : 0) - 1;
		const unsigned off = 3 - (start / 32);
		const unsigned shift = start & 31;
		uint32_t res = raw_bits[off] >> shift;
		if(size + shift > 32) {
			res |= raw_bits[off - 1] << ((32 - shift) % 32);
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

} // namespace Storage::SD

String toString(Storage::SD::CSD::Structure structure);
