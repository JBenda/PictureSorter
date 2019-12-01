#include "HuffTable.hpp"

#include "Util.hpp"
#include <array>
#include <istream>
#include <ostream>
#include <bitset>

#ifdef DEBUG
#include <cassert>
#include <iostream>
#endif
namespace fJPG {
	void sub(std::size_t& count, std::size_t end, std::ostream& os, std::size_t depth, std::bitset<16> path, const std::vector<HuffTable::Node>& tree, const HuffTable::Node& node) {
		if (count == end) return;

		if (!node.isLeave()) {
			sub(count, end, os, depth + 1, path.set(depth, false), tree, tree[node.l]);
			sub(count, end, os, depth + 1, path.set(depth, true), tree, tree[node.r]);
		} else {
			++count;
			os << "0b";
			for (std::size_t i = 0; i < depth; ++i) {
				os << (path.test(i) ? '1' : '0');
			}
			os << "\t-> " << (int)(node.r) << '\n';
		}
	}

	std::ostream& operator<<(std::ostream& os, const HuffTable& h) {
		std::size_t count = 0;
#ifdef DEBUG
		sub(count, h.d_numCodes, os, 0, std::bitset<16>(0), h.tree, h.tree[0]);
#else
		os << "HuffTable printing: only in DEBUG mode supported!\n";
#endif
		return os;
	}
	HuffTable::Header HuffTable::ParseHeader(std::istream& is){
		Header hh;
		uint8_t info;
		Read(is, info);
		hh.id = info & 0b1111;
		hh.isAC = info & 0b10000;
#ifdef DEBUG
		// bits 5…7 must be zero
		assert((info & 0b11100000) == 0);
#endif
		return hh;
	}

	std::istream& operator >>(std::istream& is, HuffTable& h) {
		uint8_t codes[16];
		Read(is, codes, 16);

		std::size_t nCodes = 0;
		for(std::size_t i = 0; i < 16; ++i) {
			nCodes += codes[i];
		}
#ifdef DEBUG
		h.d_numCodes = nCodes;
#endif

		uint8_t values[256];
		Read(is, values, nCodes);
		uint8_t *value = values;
		
		h.tree.resize(256 * 8 + 36);

		LazyTreePreIterator<16> itr(h.tree.data());	
		for(std::size_t f = 0; f < 16; ++f) {
			for(std::size_t i = 0; i < codes[f]; ++i) {
				while (itr.floor() != f + 1) {
#ifdef DEBUG
					assert(itr.floor() < f + 1);
					if (!itr) {
						std::cout << h << '\n';
						assert(false /*failed to fill tree*/);
					}
#endif
					++itr;
				}
#ifdef DEBUG
					assert(nCodes-- > 0);

#endif
					itr.set(*(value++));
			}
		}
		h.tree.resize(itr.size());
		return is;
	}
}
