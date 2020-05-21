#include "ImageData.hpp"

#define _USE_MATH_DEFINES
#include <cmath>
#include <bit>
#include <algorithm>

void ImageData::calculateMd5Hash(const std::filesystem::path& path) {
		std::ifstream file(path, std::ios::binary);

		std::array<char, 2048> buffer;
		md5_t md5Helper;
		md5_init(&md5Helper);
		while(file) {
			file.read(buffer.data(), buffer.size());
			md5_process(&md5Helper, buffer.data(), file.gcount()); 
		}
		md5_finish(&md5Helper, &_md5Hash);
}

float constexpr sqrtNewtonRaphson(float x, float curr, float prev) {
	return curr == prev
			? curr
			: sqrtNewtonRaphson(x, 0.5 * (curr + x / curr), curr);
}

constexpr float const_sqrt(float x) { return sqrtNewtonRaphson(x, x, 0); }

float dct_matrix(int x, int y) { // TODO: make constexpr 
	const float c1 = std::sqrt(2.f/32.f); // const_sqrt(2.f / 32.f);
	return c1 * cos((M_PI / 2.f / 32.f) * static_cast<float>(y * (2 * x + 1)));
}

float dct_t_matrix(int x, int y) { // TODO: make constexpr
	return dct_matrix(y,x);
}

// based on phash.org dct hash
PHash::PHash(const color_t* Y, std::size_t w, std::size_t h) {
	// mean filter 7 x 7
	// resize 32 x 32 nearest neighbor
	// dct_matrix (m) 32x32 1 / sqrt(32) 
	// c1 = sqrt(2 / N)
	// m(x,y) = c1 * cos((PI / 2 / N) * y * (2 * x + 1))
	// mT = transpose(m) 
	// dctImg = m * img * mT
	// subsec = crop (1,1,8,8) unroll ('x')
	// med = subsec.median [fold by mean filter]
	// hash[i] = subsec[i] > median
	
	std::array<float, 32*32> img;
	float wRatio = static_cast<float>(w - 6) / 32.f;
	float hRatio = static_cast<float>(h - 6) / 32.f;
	
	for (int x = 0; x < 32; ++x) {
		for (int y = 0; y < 32; ++y) {
				/*int cx = x * wRatio;
				int cy = y * hRatio;
				float& p = img[x + y * 32];
				int offset = cx + cy * w;
				for (int xs = 0; xs < 7; ++xs) {
					for (int xy = 0; xy < 7; ++xy) {
						p += Y[offset + xs + xy * w];
					}
				}
				p /= 49.f; */
          img[x + y * 32] = Y[static_cast<int>(x * wRatio + y * hRatio * w)];
		}
	}

	std::array<float, img.size()> buffer;
	for (int x = 0; x < 32; ++x) {
		for (int y = 0; y < 32; ++y) {
			buffer[x + y * 32] = 0;
			for (int i = 0; i < 32; ++i) {
				buffer[x + y * 32] += img[i + 32 * y] * dct_matrix(x, i);
			}	
		}
	}

	for (int x = 0; x < 32; ++x) {
		for (int y = 0; y < 32; ++y) {
			img[x + y * 32] = 0;
			for (int i = 0; i < 32; ++i) {
				img[x + y * 32] += buffer[i + 32 * y]  * dct_t_matrix(x, i);
			}
		}
	}

	std::array<float, 64> streak;
	std::size_t px = 0;
	for (int x = 1; x < 9; ++x) {
		for (int y = 1; y < 9; ++y) {
			streak[px++] = img[x + y * 32];
		}
	}
    float median;
    {
      std::array<float, 64> list = streak;
      std::sort(list.begin(),list.end());
      median = list[32];
    }
	for (int i = 0; i < 64;) {
		for(auto j = 0; j < _phash.size(); ++j) {
			_phash[j] = 0;
			for( int k = 0; k < 8*sizeof(decltype(_phash)::value_type); ++k, ++i) {
				if (streak[i] > median) {
					_phash[j] |= (0x01 << k); 
				}
			}
		}
	}
}

int PHash::hammingDistance(const PHash& hash) const {
  auto litr = hash._phash.cbegin(), 
       ritr = _phash.cbegin();
  int diff = 0;
  while(ritr != _phash.cend()) {
    diff +=  std::__popcount<unsigned int>((*litr)^(*ritr));
    ++litr; ++ritr;
  }
  return diff;
}

std::ostream& operator<<(std::ostream& os, const PHash& hash) {
  constexpr int len = sizeof(hash._phash[0]) * 8;
  auto itr = hash._phash.cbegin();
  for(int i = 0; i < 64; ++i) {
    os << (0 != (*itr & (0x1 << (i % len))));
    if (i%len == len-1) { ++itr; }
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, const ImageData& img) {
	return os;
} 

std::istream& operator>>(std::istream& is,  ImageData& img) {
	return is;
}
