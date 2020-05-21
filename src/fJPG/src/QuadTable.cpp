#include "QuadTable.hpp"
#include "Util.hpp"
#include <array>
#include <cstdint>
namespace fJPG {
	std::istream& operator>>(std::istream& is, QuadTable& q) {
		uint8_t info;
		Read(is, info);
		q._id = info & 0b1111;
		uint8_t precission = info & 0xF0
			? 2
			: 1;
		if (precission == 1) {
			uint8_t value;
			Read(is, value);
			q._dc = value;
		} else {
			std::array<uint8_t, 2> value;
			Read(is, value);
			q._dc = (value[0] << 8) | value[1];
		}
		Skip(is, static_cast<std::size_t>(precission) * 63);
		return is;
	}
}
