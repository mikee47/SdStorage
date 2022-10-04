#pragma once

#include <Print.h>

namespace Storage::SD
{
struct __attribute__((packed)) CID {
	uint8_t mid;		  ///< Manufacturer ID
	char oid[2];		  ///< OEM / Application ID
	char pnm[5];		  ///< Product name
	uint8_t prv;		  ///< Product revision
	uint32_t psn;		  ///< Product serial number
	uint16_t mdt;		  ///< Manufacturing date
	uint8_t not_used : 1; ///< Always 1
	uint8_t crc : 7;	  ///< 7-bit checksum

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

} // namespace Storage::SD
