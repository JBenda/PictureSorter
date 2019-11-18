#pragma once
#include <iosfwd>

namespace fJPG {
	class QuadTable {
	public:
		int dc() const { return _dc; }
		friend std::istream& operator>>(std::istream& is, QuadTable& q);
		int id() const { return _id; }
	private:
		int _dc{ 0 }; ///< quantization value[0] (for dc value)
		int _id{ -1 };
	};
}