#pragma once

#include <iosfwd>
#include <array>
#include <vector>
#include <string>
#include <cmath>
#include <cassert>

namespace fJPG {
	typedef unsigned char color_t;

	template<typename T>
	struct Dim {
		template<typename T>
		explicit operator Dim<T>() const {
			return Dim<T>( static_cast<T>( x ), static_cast<T>( y ) );
		}
		Dim(T vx, T vy) : x{ vx }, y{ vy }{}
		Dim() = default;
		T x{0};
		T y{0};
	} ;
	
	enum struct ColorEncoding {
		BW8,
		RGB8,
		YCrCb8,
		Frequenz
	};

	template<ColorEncoding>
	class Picture;

	class Imag {
		void generateEuler() const {
			constexpr double pi = 3.14159265358979323846;
			if ( _euler || !_sum ) return;
			_l = std::sqrt(_r * _r + _i * _i);
			if ( _i == 0 ) {
				_phi = 0;
			} else {
				_phi = std::fmod( _r / _i + pi, 2.0 * pi );
				_phi += _phi < 0 ? pi : -pi;
			}
			assert(!std::isnan(_phi));
			_euler = true;
		}
		void generateSum() const {
			if ( _sum || !_euler ) return;
			_r = _l * std::cos( _phi );
			_i = _l * std::sin( _phi );
			_sum = true;
		}
	public:
		static Imag Euler( double l, double phi ) {
			Imag res;
			res._sum = false;
			res._l = l;
			res._phi = phi;
			return res;
		}
		static Imag Sum( double r, double i ) {
			Imag res;
			res._euler = false;
			res._r = r;
			res._i = i;
			return res;
		}
		double r() const {
			generateSum();
			return _r;
		}
		double i() const {
			generateSum();
			return _i;
		}
		double l() const {
			generateEuler();
			return _l;
		}
		double phi() const {
			generateEuler();
			return _phi;
		}
		Imag() : _euler{ true }, _sum{ true }{}
		Imag operator*( Imag& img ) const {
			generateEuler();
			img.generateEuler();
			Imag res{};
			res._l = _l * img._l;
			res._phi = _phi + img._phi;
			res._sum = false;
			return res;
		}
		Imag& operator+=( Imag & img ) {
			generateSum();
			img.generateSum();
			_euler = false;
			_r += img._r;
			_i += img._i;
			return *this;
		}
	private:
		mutable double _r{ 0 }, _i{ 0 };
		mutable double _l{ 0 }, _phi{ 0 };
		mutable bool _euler{ false };
		mutable bool _sum{ false };
	};

	template<typename itr_t>
	class Channel  {
		template<ColorEncoding>
		friend class Picture;
		Channel(itr_t begin, itr_t end) : _begin(begin), _end(end) {}

	public:
		Channel() = default;
		using iterator = itr_t;
		const itr_t& begin() const { return _begin; }
		const itr_t& end() const { return _end; }
	private:
		itr_t _begin{};
		itr_t _end{};
	} ;				


	template<ColorEncoding C>
	class Picture {
		void validateChannel(const std::size_t x) const {
			if ( x >= _channels) {
				throw std::string("channel out of bound");
			}
		}
	public:
		static constexpr ColorEncoding ENCODING = C;
		const Dim<std::size_t>& GetSize() const { return _size; }	
		Channel<std::vector<color_t>::const_iterator> GetChannel(std::size_t x) const {
			validateChannel(x);

			return Channel(_data.cbegin() + _nPx * x, _data.cbegin() + _nPx * (x + 1));
		}

		Picture() = default;
		Picture(const Picture&) = default;
		Picture& operator=(const Picture&) = default;
		Picture(Picture&&) = default;
		Picture& operator=(Picture&&) = default;
		float mean(std::size_t id) { return _mean[id];  }
		float varianze(std::size_t id) { return _varianze[id]; }
	protected:
		Picture(Dim<std::size_t> size, std::size_t channels)
			: _size{size}, _channels{channels}, _nPx{size.x * size.y}, _data(_nPx * _channels){}

		Channel<std::vector<color_t>::iterator> GetRawChannel(std::size_t x) {
			validateChannel(x)	;

			return Channel<std::vector<color_t>::iterator>(
						_data.begin() + _nPx * x,
						_data.begin() + _nPx * ( x + 1)
					);
		}
		std::array<float, 3> _mean{0};
		std::array<float, 3> _varianze{0};
	private:
		Dim<std::size_t> _size{0};
		std::size_t _channels{0};
		std::size_t _nPx{0};
		std::vector<color_t> _data{};
	} ;

	Picture<ColorEncoding::YCrCb8> Convert(std::istream& input);

	std::vector<Imag> ToFrequenz( const Picture<ColorEncoding::YCrCb8>& picture, unsigned int channel = 0 );

	std::vector<Imag> ToFrequenz( std::vector<double>& data );

	std::vector<color_t> ToNormal( const std::vector<Imag>& freq );
}
