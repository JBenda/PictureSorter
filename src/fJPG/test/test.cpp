#include "api.hpp"
#include <cassert>
#include <iostream>
#include <fstream>
#include "HuffTable.hpp"
#include "fJPG.hpp"

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

int main(int argc, char *argv[])
{
	assert(("yes", add(3, 5) == 8));

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
		huff >> table;
		assert(table.decode(0b00) == 1);
		assert(table.decode(0b10) == 2);
		assert(table.decode(0b001) == 3);
		assert(table.decode(0b0101) == 11);
		assert(table.decode(0b1101) == 4);
		assert(table.decode(0b0011) == 0);
	}{
		std::ifstream img(std::string(SAMPLE_DIR) + "/t1.jpeg", std::ios::binary);
		try{
			fJPG::Picture picture = fJPG::Convert(img);
		} catch (const std::string& msg) {
			std::cerr << "conversion failed with: " << msg << '\n';
			assert(true);
		}
	}


	return 0;
}
