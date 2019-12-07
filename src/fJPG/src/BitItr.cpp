#include "BitItr.hpp"

namespace fJPG {
	void BitItr::sync( uint8_t flag ) {
		_block = ( flag & 0xF0 ) == 0xD0 && flag < 0xD8? ( flag & 0x0F ) : flag;
	}

	void BitItr::nextByte() {
		_offset -= 8;
		_window[0] = _window[1];
		_window[1] = _window[2];
		_fillByte[0] = _fillByte[1];
		_fillByte[1] = false;

		_window[2] = fetchByte();
		
	}

	uint8_t BitItr::fetchByte() {
		uint8_t byte;
		if ( _block > 7 ) return 0x00; // FIXME: should work with 0xFF also
		Read( _is, byte );
		if ( byte == 0xFF ) {
			uint8_t flag;
			Read( _is, flag );
			if ( flag == 0x00 ) {
				return 0xFF;
			} else {
				_fillByte[1] = true;
				sync( flag );
				return fetchByte();
			}
		} else {
			return byte;
		}
	}

	uint16_t BitItr::peek16Bit() {
		return  ( _window[0] << ( 8 + _offset ) )
			+ ( _window[1] << _offset )
			+ ( _window[2] >> ( 8 - _offset ) );
	}

	BitItr& BitItr::operator++() {
		++_offset;
		if ( _offset > 7 ) nextByte();
		return *this;
	}

	BitItr& BitItr::operator+=( std::size_t x ) {
		for ( std::size_t i = 0; i < x; ++i ) { this->operator++(); }
		return *this;
	}

	uint8_t BitItr::nextHuff( const HuffTable& huff ) {
		uint16_t data;
		if ( _fillByte[0] && ( _window[0] ^ ( 0xFF >> _offset ) ) ) {
#ifdef DEBUG
			assert( _offset < 8 );
#endif // DEBUG

			_offset = 8;
			nextByte();
		}
		data = peek16Bit();
		return huff.decode( data, *this );
	}

	BitItr::BitItr( std::istream& is, const std::streampos& startData )
		: _is{ is } {
		is.seekg( startData );
		Read( is, _window );
	}
}