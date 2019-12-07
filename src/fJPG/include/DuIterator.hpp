#pragma once

#include <cstddef>
#include <vector>

#include "fJPG.hpp"

struct JPGDecomposition;
template<typename>
class Channel;


namespace fJPG {
	class DuIterator {
		void incriment();

	public:
		DuIterator( const JPGDecomposition& base, std::size_t channelId, Channel<std::vector<color_t>::iterator>& channel );

		DuIterator() = default;

		void insert( int diff );

		double mean() const;
	private:
		const Dim<uint8_t> _samp;
		const Dim<uint8_t> _mcuSamp;
		const Dim<std::size_t> _nDus;
		Channel<std::vector<color_t>::iterator> _channel;
		std::vector<color_t>::iterator _itr;
		Dim<uint8_t> _local{ 0,0 };
		Dim<std::size_t> _global{ 0,0 };
		std::size_t _sum{0};
		double _mean{-1};
		int _diff{ 0 };
#ifdef DEBUG
		std::size_t _id{ 255 };
#endif // DEBUG

	};
}