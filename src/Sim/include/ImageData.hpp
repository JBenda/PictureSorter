#pragma once

#include "Settings.hpp"
#include "md5.h"

#include <filesystem>
#include <fstream>
#include <iosfwd>
#include <sstream>
#include <string>
#include <vector>


class PHash {
public:
  PHash(const color_t* Y, std::size_t w, std::size_t h);
  PHash() {}
  int hammingDistance(const PHash& hash) const; 
  friend std::ostream& operator<<(std::ostream& os, const PHash& hash);
private:
    // hamming trash-hold = 22
    static constexpr std::size_t s_array_size =
      (8+sizeof(unsigned int)-1)/ sizeof(unsigned int);
	std::array<unsigned int, s_array_size> _phash;};

class ImageData
{
public:
	ImageData(
			const std::filesystem::path& path,
			const color_t* Y,
			const std::size_t w,
			const std::size_t h)
		: _path{path},_phash(Y,w,h) 
	{
		if ( !std::filesystem::exists(path) ) {
			std::runtime_error("file: '" + path.string() + "' don't exist");
		}

		calculateMd5Hash(path);

	}
	ImageData() {}
    std::string print() const {
      std::stringstream ss;
      char md5[33];
      md5_sig_to_string(_md5Hash.data(), md5, 33);
      ss << "Path: " << _path
        << "\nMD5: " << md5
        << "\nphash: " << _phash;
      return ss.str();
    }
	friend std::ostream& operator<<(std::ostream& os, const ImageData& img);
	friend std::istream& operator>>(std::istream& is,  ImageData& img);
	std::size_t size() const { return 0; /*TODO: add function*/ }
    const PHash& phash() const { return _phash; }
private:
	void calculateMd5Hash(const std::filesystem::path& path);
	std::filesystem::path _path;
	std::array<int, (16 + sizeof(int) - 1) / sizeof(int)> _md5Hash;
    PHash _phash;
};

