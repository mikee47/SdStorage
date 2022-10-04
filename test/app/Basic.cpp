#include <Storage/SD/CSD.h>
#include <Storage/SD/CID.h>
#include <SmingTest.h>

using namespace Storage::SD;

class BasicTest : public TestGroup
{
public:
	BasicTest() : TestGroup(_F("Basic"))
	{
	}

	void execute() override
	{
		TEST_CASE("CSD")
		{
			const uint8_t data[]{0x40, 0x0e, 0x00, 0x32, 0x5b, 0x59, 0x00, 0x00,
								 0xee, 0x7f, 0x7f, 0x80, 0x0a, 0x40, 0x40, 0x55};
			static_assert(sizeof(data) == sizeof(CSD));

			CSD csd;
			memcpy(&csd, data, sizeof(data));

			csd.bswap();

			for(auto& w : csd.raw_bits) {
				Serial << String(w, HEX, 8) << ", ";
			}
			Serial.println();

			Serial << csd << endl;

			REQUIRE_EQ(csd.structure(), CSD::Structure::v2);
			REQUIRE_EQ(csd.taac(), 14);
			REQUIRE_EQ(csd.nsac(), 0);
			REQUIRE_EQ(csd.tran_speed(), 50);
			REQUIRE_EQ(csd.ccc(), 1461);
			REQUIRE_EQ(csd.read_bl_len(), 9);
			REQUIRE_EQ(csd.read_bl_partial(), 0);
			REQUIRE_EQ(csd.write_blk_missalign(), 1);
			REQUIRE_EQ(csd.read_blk_misalign(), 0);
			REQUIRE_EQ(csd.dsr_imp(), 0);
			REQUIRE_EQ(csd.v2().size(), 32010928128);
			REQUIRE_EQ(csd.v2().c_size(), 61055);
			REQUIRE_EQ(csd.erase_blk_en(), 1);
			REQUIRE_EQ(csd.sector_size(), 127);
			REQUIRE_EQ(csd.wp_grp_size(), 0);
			REQUIRE_EQ(csd.wp_grp_enable(), 0);
			REQUIRE_EQ(csd.r2w_factor(), 2);
			REQUIRE_EQ(csd.write_bl_len(), 9);
			REQUIRE_EQ(csd.write_bl_partial(), 0);
			REQUIRE_EQ(csd.file_format_grp(), 0);
			REQUIRE_EQ(csd.copy(), 1);
			REQUIRE_EQ(csd.perm_write_protect(), 0);
			REQUIRE_EQ(csd.tmp_write_protect(), 0);
			REQUIRE_EQ(csd.file_format(), 0);
			REQUIRE_EQ(csd.wp_upc(), 0);
			REQUIRE_EQ(csd.crc(), 42);
		}

		TEST_CASE("CID")
		{
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
			const uint8_t data[]{0x1b, 0x53, 0x4d, 0x45, 0x42, 0x31, 0x51, 0x54,
								 0x30, 0xf1, 0x77, 0x5f, 0xea, 0x01, 0x1a, 0xb9};
			static_assert(sizeof(data) == sizeof(CID));

			CID cid;
			memcpy(&cid, data, sizeof(cid));
			cid.bswap();

			m_printHex("CID", &cid, sizeof(cid));
			Serial << "Card Identification Information" << endl << cid;

			REQUIRE_EQ(cid.mid, 0x1b);
			REQUIRE(memcmp(cid.oid, "SM", 2) == 0);
			REQUIRE(memcmp(cid.pnm, "EB1QT", 5) == 0);
			REQUIRE_EQ(cid.major(), 3);
			REQUIRE_EQ(cid.minor(), 0);
			REQUIRE_EQ(cid.psn, 0xf1775fea);
			REQUIRE_EQ(cid.mdt_year(), 2017);
			REQUIRE_EQ(cid.mdt_month(), 10);
			REQUIRE_EQ(cid.crc, 0x5c);
		}
	}
};

void REGISTER_TEST(basic)
{
	registerGroup<BasicTest>();
}
