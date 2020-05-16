#pragma once

#include "Settings.hpp"
#include "md5.h"

#include <iosfwd>
#include <filesystem>
#include <vector>
#include <fstream>

class ImageData
{
public:
	ImageData(
			const std::filesystem::path& path,
			const color_t* Y,
			const std::size_t w,
			const std::size_t h)
		: path{path} 
	{
		if ( !std::filesystem::exists(path) ) {
			std::runtime_error("file: '" + path.string() + "' don't exist");
		}

		calculateMd5Hash(path);

		calculatePHash(Y, w, h);

	}
	ImageData() {}
	friend std::ostream& operator<<(std::ostream& os, const ImageData& img);
	friend std::istream& operator>>(std::istream& is,  ImageData& img);
	std::size_t size() const { return 0; /*TODO: add function*/ }

private:
	void calculateMd5Hash(const std::filesystem::path& path);
	void calculatePHash(const color_t* Y, std::size_t w, std::size_t h);
	std::filesystem::path path;
	std::array<int, (16 + sizeof(int) - 1) / sizeof(int)> _md5Hash;
	std::array<int,(8 + sizeof(int) - 1) / sizeof(int)> _phash; // hamming trash-hold = 22
};

