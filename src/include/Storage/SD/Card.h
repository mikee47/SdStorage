/*
Author: (github.com/)ADiea
Project: Sming for ESP8266 - https://github.com/anakod/Sming
License: MIT
Date: 15.07.2015
Descr: Low-level SDCard functions
*/
#pragma once

#include <Storage/Disk/BlockDevice.h>
#include <HSPI/Device.h>
#include "CSD.h"
#include "CID.h"

namespace Storage::SD
{
class SpiDevice : public HSPI::Device
{
public:
	using HSPI::Device::Device;

	HSPI::IoModes getSupportedIoModes() const override
	{
		return HSPI::IoMode::SPI;
	}
};

class Card : public Disk::BlockDevice
{
public:
	/*
	 * Whilst SD V1.XX permits misaligned and partial block reads, later versions do not
	 * and require transfers to be aligned to, and in multiples of, 512 bytes.
	 */

	Card(const String& name, HSPI::Controller& controller) : BlockDevice(), name(name), spi(controller)
	{
	}

	~Card()
	{
		end();
	}

	explicit operator bool() const
	{
		return initialised;
	}

	/**
	 * @brief Initialise the card
	 * @param chipSelect
	 * @param freq SPI frequency in Hz, use 0 for maximum supported frequency
	 */
	bool begin(HSPI::PinSet pinSet, uint8_t chipSelect, uint32_t freq = 0);

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
	bool rcvr_datablock(void* buff, size_t btr);
	bool xmit_datablock(const void* buff, uint8_t token);
	uint8_t send_cmd(uint8_t cmd, uint32_t arg);
	bool send_cmd_with_retry(uint8_t cmd, uint32_t arg, uint8_t requiredResponse, unsigned maxAttempts);

	CString name;
	SpiDevice spi;
	CSD mCSD;
	CID mCID;
	bool initialised{false};
	uint8_t cardType; ///< b0:MMC, b1:SDv1, b2:SDv2, b3:Block addressing
};					  // namespace SD

} // namespace Storage::SD
