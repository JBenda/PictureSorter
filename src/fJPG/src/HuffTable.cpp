#include "HuffTable.hpp"

#include "Util.hpp"
#include <array>

#ifdef DEBUG
#include <cassert>
#endif

namespace fJPG {
	uint8_t HuffTable::decode(uint16_t data) {
		Node n = tree[0]	;
#ifdef DEBUG
		uint8_t c = 0;
#endif
		do {
			n = data & (0x01 << 15)
				? tree[n.r]
				: tree[n.l];
			data <<= 1;
#ifdef DEBUG
			++c;
			assert(c < 16);
#endif
		} while(n.l);
		return n.r;
	}

	std::istream& operator >>(std::istream& is, HuffTable& h) {
		uint8_t info;
		Read(is, info);
		uint8_t id = info & 0b111;
		bool isAC = info & 0b1000;
#ifdef DEBUG
		// bits 5…7 must be zero
		assert((info & 0b11110000) == 0);
#endif

		uint8_t codes[16];
		Read(is, codes, 16);

		std::size_t nCodes = 0;
		for(std::size_t i = 0; i < 16; ++i) {
			nCodes += codes[i];
		}
		uint8_t values[256];
		Read(is, values, nCodes);
		uint8_t *value = values;

		LazyTreePreIterator<15> itr(h.tree);	
		for(std::size_t f = 0; f < 16; ++f) {
			for(std::size_t i = 0; i < codes[f]; ++i) {
				while(itr.floor() != f) ++itr;
					itr.set(*(value++));
#ifdef DEBUG
					assert(nCodes-- > 0);
#endif
			}
		}
		return is;
	}
}
