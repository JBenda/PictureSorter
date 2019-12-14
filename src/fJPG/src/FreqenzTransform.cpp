#include "fJPG.hpp"

#include <cmath>
#include <vector>

#include <cassert>



namespace fJPG {

	constexpr double PI = 3.14159265358979323846;

	
	template<typename T>
	Imag GetFrequenz( const Dim<unsigned int>& fr, const Dim<unsigned int>& size, const T& data, unsigned int offset = 0) {
		Imag sum{};
		const double n = static_cast<double>( size.x );
		auto itr = data.begin();
		for ( Dim<unsigned int> pos{ 0,0 }; pos.y < size.y; ++pos.y ) {
			for ( pos.x = 0; pos.x < size.x; ++pos.x ) {
				double phi = -2.0 * PI * ( static_cast<double>( fr.x * pos.x + fr.y * pos.y ) ) / n;
				sum += Imag::Euler(static_cast<double>(*itr), phi);
				double val = static_cast<double>( *itr );
				++itr;
			}
			itr += offset;
		}
		assert(itr == data.end());
		return sum;
	}

	std::vector<Imag> ToFrequenz( const Picture<ColorEncoding::YCrCb8>& picture, unsigned int channelId ) {
		double sumR = 0, sumI = 0;
		Dim<unsigned int> pos;
		Dim<unsigned int> size = static_cast<Dim<unsigned int>>( picture.GetSize() );

		if ( size.x != size.y ) {
			size.x = std::min( size.x, size.y );
			size.y = size.x;
		}
		std::vector<Imag> result( size.x * size.y );
		auto channel = picture.GetChannel( channelId );
		auto itr = result.begin();
		for ( pos.y = 0; pos.y < size.y; ++pos.y ) {
			for ( pos.x = 0; pos.x < size.x; ++pos.x ) {
				*( itr++ ) = GetFrequenz( pos, size, channel, picture.GetSize().x - size.x );
			}
		}
		assert( itr == result.end() );

		return result;
	}
	
	std::vector<Imag> ToFrequenz(std::vector<double>& data) {
		std::vector<Imag> result( data.size() );
		auto channel = data;
		auto itr = result.begin();
		unsigned int n = std::sqrt( data.size() );
		for ( Dim<unsigned int>pos(0,0); pos.y < n; ++pos.y ) {
			for ( pos.x = 0; pos.x < n; ++pos.x ) {
				*(itr++) = GetFrequenz( pos, Dim(n,n), channel);
			}
		}
		assert(itr == result.end());

		return result;
	}

	color_t GetColor( const Dim<unsigned int>& px, const unsigned int n, const std::vector<Imag>& freq ) {
		double colorSum = 0;
		auto itr = freq.begin();
		for ( Dim<unsigned int> pos{ 0,0 }; pos.y < n; ++pos.y ) {
			for ( pos.x = 0; pos.x < n; ++pos.x ) {
				double phi = 2.0 * PI * static_cast<double>( px.x * pos.x + px.y * pos.y ) / static_cast<double>( n );
				colorSum += (*itr * Imag::Euler( 1, phi )).r();
				++itr;
			}
		}

		assert( itr == freq.end() );
		double result = colorSum / (static_cast<double>( n )* static_cast<double>( n ));
		if ( result < 0 ) result = 0;
		if ( result > 255 ) result = 255;
		// assert( result > 0 && result < 256 );
		return static_cast<double>( result );
	}

	std::vector<color_t> ToNormal( const std::vector<Imag>& freq ) {
		std::vector<color_t> result( freq.size() );
		auto itr = result.begin();
		unsigned int n = static_cast<unsigned int>(std::sqrt(freq.size()));
		for ( Dim<unsigned int> pos{ 0,0 }; pos.y < n; ++pos.y ) {
			for ( pos.x = 0; pos.x < n; ++pos.x ) {
				*( itr++ ) = GetColor( pos, n, freq );
			}
		}
		return result;
	}
}