#include "api.hpp"
#include <cassert>
#include <iostream>
#include <fstream>
#include "fJPG.hpp"

#ifndef SAMPLE_DIR
#define SAMPLE_DIR "."
#endif // !SAMPLE_DIR


int main(int argc, char* argv[]) {
	{
		std::ifstream img(std::string(SAMPLE_DIR) + "/t1.jpeg", std::ios::binary);
		try {
			fJPG::Picture picture = fJPG::Convert(img);
			decltype(picture) const& pic = picture;
			std::size_t x = 0;
			for (auto c : pic.GetChannel(0)) {
				std::cout << (int)c << '\t';
				if (++x == pic.GetSize().x) {
					std::cout << '\n';
					x = 0;
				}
			}
		}
		catch (const std::string & msg) {
			std::cerr << "conversion failed with: " << msg << '\n';
			assert(true);
		}
	}


	return 0;
}
