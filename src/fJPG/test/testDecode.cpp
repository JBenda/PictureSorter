#include "api.hpp"
#include <cassert>
#include <iostream>
#include <fstream>
#include "fJPG.hpp"

#ifndef SAMPLE_DIR
#define SAMPLE_DIR "."
#endif // !SAMPLE_DIR

bool compare(const std::string& p1, const std::string& p2) {
	std::ifstream
		f1(p1, std::ios::binary | std::ios::ate),
		f2(p2, std::ios::binary | std::ios::ate);
	if (!(f1 && f2) || !(f1.tellg() == f2.tellg())) { return false; }
	f1.seekg(0, std::ios::beg);
	f2.seekg(0, std::ios::beg);

	return std::equal(
		std::istreambuf_iterator<char>(f1.rdbuf()),
		std::istreambuf_iterator<char>(),
		std::istreambuf_iterator<char>(f2.rdbuf())
	);

}

int main(int argc, char* argv[]) {
	{
		std::string base(SAMPLE_DIR);
		std::ifstream img(base + "/t1.jpeg", std::ios::binary);
		try {
			fJPG::Picture picture = fJPG::Convert(img);
			img.close();
			decltype(picture) const& pic = picture;
			std::ofstream out(base + "/t1.pgm", std::ios::binary);
			out << "P5\n" << pic.GetSize().x << ' ' << pic.GetSize().y << "\n255\n";
			for (auto c : pic.GetChannel(0)) {
				out << c;
			}
			out.close();
			assert(compare(base + "/t1.pgm", base + "/t1.pgm.res"));

		}
		catch (const std::string & msg) {
			std::cerr << "conversion failed with: " << msg << '\n';
			assert(true);
		}
	}


	return 0;
}
