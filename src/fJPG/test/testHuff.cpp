#include <cassert>
#include <iostream>
#include <fstream>
#include "HuffTable.hpp"

#ifndef SAMPLE_DIR
#define SAMPLE_DIR "."
#endif // !SAMPLE_DIR


std::ostream& operator<<(std::ostream& os, const fJPG::HuffTable::Node& node) {
	os << '(' << (int)node.l << '|' << (int)node.r << ')';
	return os;
}
bool operator!=(const fJPG::HuffTable::Node& n1, const fJPG::HuffTable::Node& n2) {
	return n1.l != n2.l || n1.r != n2.r;
}

int main(int argc, char *argv[]) {

	fJPG::HuffTable::Node tree[256];
	fJPG::LazyTreePreIterator<15> itr(tree);
	uint8_t codes[] = {0, 2, 1, 3};
	uint8_t values[] = {1, 2, 3, 11, 4, 0};
	uint8_t *value = values;
	for(std::size_t f = 0; f < 4; ++f) {
		for(std::size_t i = 0; i < codes[f]; ++i) {
			while(itr.floor() != f+1) { assert(++itr);}
			itr.set(*(value++));
		}
	}
	fJPG::HuffTable::Node res[] = {
								{1,4}, 
				{2,3}, 
		{0,1}, {0,2}, 	{5,10}, 
								{6,7}, 
						{0,3}, {8,9},
								{0,11},{0,4},  {11,0},
													{12,13},
												{0,0}
	};	

	for(int i = 0; i < sizeof(res) / sizeof(*res); ++i) {
		if(tree[i] != res[i]) {
			std::cerr << "mismatch: " << tree[i] << " expected: " << res[i] << '\n';
			assert(true);
		}
	}
	{
		fJPG::HuffTable table{};
		std::ifstream huff(std::string(SAMPLE_DIR) + "/huffTable.data", std::ios::binary);
		fJPG::HuffTable::ParseHeader(huff);
		huff >> table;
		std::size_t count = 0;
		assert(table.decode(0b00   << 14, count) == 1);
		assert(table.decode(0b01   << 14, count) == 2);
		assert(table.decode(0b100  << 13, count) == 3);
		assert(table.decode(0b1010 << 12, count) == 11);
		assert(table.decode(0b1011 << 12, count) == 4);
		assert(table.decode(0b1100 << 12, count) == 0);
	}
	return 0;
}
