#pragma once

#include <iosfwd>
#include <array>
#include <vector>
#include <string>

namespace fJPG {
	class Picture;
	typedef unsigned char color_t;

	template<typename T>
	struct Dim {
		T x{0};
		T y{0};
	} ;

	template<typename itr_t>
	class Channel  {
		friend Picture;
		Channel(itr_t begin, itr_t end) : _begin(begin), _end(end);
	public:
		const itr_t& begin() const { return _begin; }
		const itr_t& end() const { return _end; }
	private:
		itr_t _begin;
		itr_t _end;
	} ;				

	class Picture {
		void validateChanel(const std::size_t x) {
			if ( x >= _channnels) {
				throw std::string("channel out of bound") + #__FILE__##__LINE__;
			}
		}
	public:
		const Dim& GetSize() const { return _size; }	
		Channel<std::vector<color_t>::const_iterator> GetChanel(std::size_t x) const {
			validateChanel(x);

			return Chanel(_data.cbegin() + _nPx * x, _data.cbegin() + _nPx * (x + 1));
		}

		Picture() = default;
		Picture(const Picture&) = delete;
		Picture& operator=(const Picture&) = delete;
	protected:
		Picture(Dim<std::size_t> size, std::size_t channels)
			: _size{size}, _channels{channels}, _nPx{size.x * size.y}, _data(_nPx * _channels){}

		Channel<std::vector<color_t>::iterator> GetChanel(std::size_t x)	{
			validateChanel(x)	;

			return Chanel(
						_data.begin() + _nPx * x,
						_data.begin() + _nPx * ( x + 1)
					);
		}
	private:
		Dim<std::size_t> _size{0};
		std::size_t _channnels{0};
		std::size_t _nPx{0};
		std::vector<color_t> _data(0);
	} ;

	Picture Convert(std::istream& input);
}
