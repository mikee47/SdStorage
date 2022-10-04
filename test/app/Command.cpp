#include <Storage/SD/Card.h>
#include <Storage/Disk/SectorBuffer.h>
#include <Storage/Disk/PartInfo.h>
#include <SmingTest.h>

// Chip selects independent of SPI controller in use
#ifdef ARCH_ESP32
#define PIN_CARD_CS 21
#else
// Esp8266 cannot use GPIO15 as this affects boot mode
#define PIN_CARD_CS 5
#endif

using namespace Storage::SD;

class CommandTest : public TestGroup
{
public:
	CommandTest() : TestGroup(_F("Commands")), card("card1", SPI)
	{
		REQUIRE(Storage::registerDevice(&card));
	}

	~CommandTest()
	{
	}

	void execute() override
	{
		REQUIRE(card.begin(PIN_CARD_CS));

		Serial << "CSD" << endl << card.csd << endl;
		Serial << "CID" << endl << card.cid;
		for(auto part : card.partitions()) {
			Serial << part << endl;
		}

		const auto sectorSize = card.getSectorSize();
		const auto sectorCount = card.getSectorCount();

		// Try a few random sectors
		static constexpr size_t SECTOR_COUNT{4};
		const size_t bufSize = SECTOR_COUNT * sectorSize;
		Storage::Disk::SectorBuffer buffer1(sectorSize, SECTOR_COUNT);
		Storage::Disk::SectorBuffer buffer2(sectorSize, SECTOR_COUNT);
		REQUIRE(buffer1 && buffer2);
		for(unsigned n = 0; n < 10; ++n) {
			auto sector = os_random() % (sectorCount - SECTOR_COUNT);
			auto offset = sector * sectorSize;

			Serial << endl << "** Test sectors " << sector << " - " << sector + SECTOR_COUNT - 1 << endl;

			TEST_CASE("Write/Read sectors")
			{
				os_get_random(buffer1.get(), buffer1.size());
				REQUIRE(card.write(offset, buffer1.get(), bufSize));

				buffer2.clear();
				REQUIRE(card.read(offset, buffer2.get(), bufSize));
				REQUIRE(buffer1 == buffer2);

				// Ensure region is not empty
				buffer1.clear();
				CHECK(buffer1 != buffer2);

				// Erase the region
				REQUIRE(card.erase_range(offset, bufSize));

				// Confirm region is now empty
				buffer2.fill(0xaa);
				REQUIRE(card.read(offset, buffer2.get(), bufSize));
				buffer1.clear();
				REQUIRE(buffer1 == buffer2);
			}
		}
	}

private:
	Card card;
};

void REGISTER_TEST(command)
{
	registerGroup<CommandTest>();
}
