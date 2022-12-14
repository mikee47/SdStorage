/*
Author: (github.com/)ADiea
Project: Sming for ESP8266 - https://github.com/anakod/Sming
License: MIT
Date: 15.07.2015
Descr: Low-level SDCard functions
*/
#pragma once

#include <Storage/Disk/BlockDevice.h>
#include <SPIBase.h>
#include "CSD.h"
#include "CID.h"

namespace Storage::SD
{
class Card : public Disk::BlockDevice
{
public:
	/*
	 * Whilst SD V1.XX permits misaligned and partial block reads, later versions do not
	 * and require transfers to be aligned to, and in multiples of, 512 bytes.
	 */

	Card(const String& name, SPIBase& spi) : BlockDevice(), name(name), spi(spi)
	{
	}

	~Card()
	{
		end();
	}

	/**
	 * @brief Initialise the card
	 * @param chipSelect
	 * @param freq SPI frequency in Hz, use 0 for maximum supported frequency
	 */
	bool begin(uint8_t chipSelect, uint32_t freq = 0);

	void end();

	/* Storage Device methods */

	String getName() const override
	{
		return name.c_str();
	}

	uint32_t getId() const
	{
		return 0;
	}

	Type getType() const
	{
		return Type::sdcard;
	}

	size_t getBlockSize() const override
	{
		return size_t(mCSD.sector_size() + 1) << sectorSizeShift;
	}

	const CID& cid{mCID};
	const CSD& csd{mCSD};

protected:
	bool raw_sector_read(storage_size_t address, void* dst, size_t size) override;
	bool raw_sector_write(storage_size_t address, const void* src, size_t size) override;
	bool raw_sector_erase_range(storage_size_t address, size_t size) override;
	bool raw_sync() override;

private:
	uint8_t init();
	bool wait_ready();
	void deselect();
	bool select();
	bool rcvr_datablock(void* buff, size_t btr);
	bool xmit_datablock(const void* buff, uint8_t token);
	uint8_t send_cmd(uint8_t cmd, uint32_t arg);

	CString name;
	SPIBase& spi;
	CSD mCSD;
	CID mCID;
	uint8_t chipSelect{255};
	bool initialised{false};
	uint8_t cardType; ///< b0:MMC, b1:SDv1, b2:SDv2, b3:Block addressing
};					  // namespace SD

} // namespace Storage::SD
