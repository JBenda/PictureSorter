#pragma once

#include <istream>
#include <type_traits>
#ifdef DEBUG
#include <cassert>
#include <iostream>
#endif

namespace fJPG {

	template<typename T>
	void Read(std::istream& is, T& ref) {
		is.read(reinterpret_cast<char*>(&ref), sizeof(T));
#ifdef DEBUG
		assert(!is.fail());
#endif
	}
	template<typename T>
	void Read(std::istream& is, T* ptr, std::size_t n) {
		is.read(reinterpret_cast<char*>(ptr), sizeof(*ptr)* n);
#ifdef DEBUG
		assert(!is.fail());
#endif // DEBUG

	}
	uint16_t ReadLen(std::istream& is);
	void Skip(std::istream& is, std::size_t n);

}
