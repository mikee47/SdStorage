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

} // namespace Storage::SD
