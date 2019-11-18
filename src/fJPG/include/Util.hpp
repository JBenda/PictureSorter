#pragma once

#include <istream>
#include <type_traits>
#ifdef DEBUG
#include <cassert>
#endif

namespace fJPG {

	template<typename T>
	void Read(std::istream& is, T& ref, std::size_t n = 1) {
		if constexpr (std::is_pointer<T>::value) {
			is.read(reinterpret_cast<char*>(ref), sizeof(std::remove_pointer<T>) * n);	
		} else {
			is.read(reinterpret_cast<char*>(&ref), sizeof(T));
		}
#ifdef DEBUG
		assert(is.fail());
#endif
	}
	void Skip(std::istream& is, std::size_t n) {
		is.seekg(n, std::ios::cur);
	}

}
