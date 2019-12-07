#pragma once

#include <array>
#include <cinttypes>
#include <iosfwd>

#include "Util.hpp"
#include "HuffTable.hpp"

namespace fJPG {
	class BitItr {
		void sync( uint8_t flag );
		void nextByte();
		uint16_t peek16Bit();
		uint8_t fetchByte();

	public:
		uint8_t	GetResetCount() { return _block; }
		BitItr& operator++();

		BitItr& operator+=( std::size_t x );

		uint8_t nextHuff( const HuffTable& huff );

		
		int16_t read( std::size_t len ) {
			if ( len == 0 || len > 32 ) return 0;
			if ( len > 16 ) {
				throw "to long!";
			}
			int16_t res = peek16Bit();
			*this += len;

			res = ( res >> ( 16 - len ) )& ( 0xFFFF >> ( 16 - len ) );

			if ( res >> ( len - 1 ) == 0 ) { // signed
				res -= ( 0b1 << len ) - 1;
			}
			return res;
		}
		BitItr( std::istream& is, const std::streampos& startData );

	private:
		std::array<uint8_t, 3> _window{ 0,0,0 };
		std::array<bool, 2> _fillByte{ false, false };
		uint8_t _offset{ 0 };
		uint8_t _block{ 0 };
		std::istream& _is;
	};
}