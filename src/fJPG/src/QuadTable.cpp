#include "QuadTable.hpp"
#include "Util.hpp"
#include <cstdint>
namespace fJPG {
	std::istream& operator>>(std::istream& is, QuadTable& q) {
		uint8_t info;
		Read(is, info);
		q._id = info & 0b1111;
		uint8_t precission = info & 0bF0
			? 2
			: 1;
		uint8_t value[] = {0, 0};
		Read(is, value, precission);
		Skip(is, precission * 63);
		_dc = precission == 1
			? value[0]
			: (static_cast<uint16_t>(value[0]) << 8)
			& static_cast<uint16_t>(value[1]);
		return is;
	}
}