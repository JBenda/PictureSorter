#include "Util.hpp"

#include <array>

namespace fJPG {
	uint16_t ReadLen(std::istream& is) {
		std::array<uint8_t, 2> bytes;
		Read(is, bytes);
		return (bytes[0] << 8) + bytes[1];
	}
	void Skip(std::istream& is, std::size_t n) {
		is.seekg(n, std::ios::cur);
	}
	void Skip( std::istream& is, unsigned int n ) {
		is.seekg( n, std::ios::cur );
	}
}
