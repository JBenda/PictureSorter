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
		template<typename U>
		explicit operator Dim<U>() const {
			return Dim<T>( static_cast<U>( x ), static_cast<U>( y ) );
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
        const typename itr_t::value_type* data() const { return &(*_begin);}
        std::size_t size() const { return _end - _begin; }
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
			: _size(size), _channels{channels}, _nPx{size.x * size.y}, _data(_nPx * _channels){}

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
		Dim<std::size_t> _size{};
		std::size_t _channels{0};
		std::size_t _nPx{0};
		std::vector<color_t> _data{};
	} ;

	Picture<ColorEncoding::YCrCb8> Convert(std::istream& input);
}
