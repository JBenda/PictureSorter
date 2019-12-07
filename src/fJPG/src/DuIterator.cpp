#include <cassert>
#include <vector>

#ifdef DEBUG
#include <iostream>
#endif // DEBUG


#include "DataCollection.hpp"
#include "DuIterator.hpp"


namespace fJPG {
	DuIterator::DuIterator( const JPGDecomposition& base, std::size_t channelId, Channel<std::vector<color_t>::iterator>& channel )
		:
		_samp{ base.channelInfos[channelId].sampH, base.channelInfos[channelId].sampV },
		_mcuSamp{ base.mcuSamp },
		_nDus{ base.nDus },
		_channel{ channel },
		_itr{ _channel.begin() }{
#ifdef DEBUG
		_id = channelId;
		assert( _channel.end() - _channel.begin() == base.nDus.x * base.nDus.y );

		uint8_t ratio;
		assert( (ratio = _mcuSamp.x / _samp.x, !( ratio == 0 ) && !( ratio& ( ratio - 1 ) ) ) );
		assert( (ratio = _mcuSamp.y / _samp.y, !( ratio == 0 ) && !( ratio& ( ratio - 1 ) ) ) );
#endif // DEBUG

	}


	void DuIterator::incriment() {
		
		++_local.x;
		++_itr;

		if ( _local.x == _mcuSamp.x ) {
			++_local.y;
			_local.x = 0;
			if ( _local.y == _mcuSamp.y ) {
				_local.y = 0;
				_global.x += _mcuSamp.x;
				if ( _global.x >= _nDus.x ) {
					_global.x = 0;
					_global.y += _mcuSamp.y;
				} else {
					_itr -= ( _mcuSamp.y - 1 ) * _nDus.x;
				}
			} else {
				_itr += _nDus.x - _mcuSamp.x;
			}
		} 
	}

	void DuIterator::insert( int diff ) {
		_diff += diff;
#ifdef DEBUG
		if ( _samp.x == 0 ) {
			assert( false );
		}
#endif
		do {
#ifdef DEBUG
			assert( _itr != _channel.end() );
			assert( *_itr == 0 );
			assert( _diff / 8 + static_cast<int>( ( std::numeric_limits<color_t>::max() + 1 ) / 2 ) < 256
					&& _diff / 8 + static_cast<int>( ( std::numeric_limits<color_t>::max() + 1 ) / 2 ) >= 0 );
#endif // DEBUG
			* _itr = _diff / 8 + static_cast<int>( ( std::numeric_limits<color_t>::max() + 1 ) / 2 );
			incriment();
#ifdef DEBUG
			if ( _global.y == 0 && _global.x == 0 ) {
				// assert( _itr == _channel.begin() || _itr == _channel.end() );
			}
#endif // DEBUG

		} while ( _itr != _channel.end() && ( _local.x >= _samp.x || _local.y >= _samp.y ) );
	}
}