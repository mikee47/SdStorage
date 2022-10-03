#include "include/Storage/SD/CSD.h"

String toString(Storage::SD::CSD::Structure structure)
{
	using Structure = Storage::SD::CSD::Structure;

	switch(structure) {
	case Structure::v1:
		return "v1";
	case Structure::v2:
		return "v2";
	case Structure::v3:
		return "v3";
	default:
		return F("INVALID");
	}
}

namespace Storage::SD
{
uint64_t CSD::getSize() const
{
	switch(structure()) {
	case CSD::Structure::v1:
		return v1().size();
	case CSD::Structure::v2:
		return v2().size();
	case CSD::Structure::v3:
		return v3().size();
	default:
		return 0;
	}
}

size_t CSD::printTo(Print& p) const
{
	size_t n{0};

#define FIELD(tag, value)                                                                                              \
	n += p.print("  ");                                                                                                \
	n += p.print(F(tag).pad(20));                                                                                      \
	n += p.print(" : ");                                                                                               \
	n += p.println(value);

#define XX(tag, ...) FIELD(#tag, csd.tag())

	auto& csd = *this;
	SDCARD_CSD_MAP_A(XX)

	switch(structure()) {
	case Structure::v1: {
		auto& csd = v1();
		FIELD("size", csd.size())
		SDCARD_CSD_MAP_B1(XX)
		break;
	}
	case Structure::v2: {
		auto& csd = v2();
		FIELD("size", csd.size())
		SDCARD_CSD_MAP_B2(XX)
		break;
	}
	case Structure::v3: {
		auto& csd = v3();
		FIELD("size", csd.size())
		SDCARD_CSD_MAP_B3(XX)
		break;
	}
	default:
		FIELD("size", _F("UNKNOWN"));
	}

	SDCARD_CSD_MAP_C(XX)

#undef XX

	return n;
}

} // namespace Storage::SD
