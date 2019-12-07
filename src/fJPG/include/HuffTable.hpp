#pragma once

#include <iosfwd>
#include <cstdint>
#include <cassert>
#include <array>
#include <bitset>
#include <vector>

namespace fJPG {
	class HuffTable {
	public:
		/**
		 *	@brief if l == 0, r = value, and note is leaf. 
		*/
		struct Node {
			using address_t = uint16_t;
			Node(address_t lh, address_t rh)
				: l{lh}, r{rh}{}
			Node() : l{0}, r{0}{}
			bool isLeave() const { return l == 0; }
			uint8_t value() const { return static_cast<uint8_t>(r); }
			address_t l;
			address_t r;
		};
		
		/**
		 *	@brief decode an aligned huffman code with length <= 16
		 *	@return decoded value
		*/
		template<typename T>
		uint8_t decode(const uint16_t data, T& itr) const {
			uint16_t value = data;
			Node n = tree[0]	;
#ifdef DEBUG
			uint8_t c = 0;
#endif
			do {
				++itr;
				if (n.r == tree.size() - 1) {
					if ( value & 0x8000 ) {
						std::cout << "leng: " << (int)c << "\n\t";
						std::cout << (int) ( tree[n.l].r ) << "|" << (int) ( tree[n.r].r ) << "\n";
						if ( c == 15 ) {
							std::cout << "\tsearch: " << std::bitset<16>( data ) << '\n';
							assert( data != 0xFFFF );
						}
					}
				}
				n = value & (0x8000)
					? tree[n.r]
					: tree[n.l];
				value <<= 1;
#ifdef DEBUG
				assert(c++ < 16);
#endif
			} while(n.l);
			return static_cast<uint8_t>(n.r);
		}
		struct Header {
			uint8_t id;
			bool isAC;
		};
		static Header ParseHeader(std::istream& is);

		friend std::ostream& operator<<(std::ostream& os, const HuffTable& h);
		friend std::istream& operator>>(std::istream& is, HuffTable& h);
		HuffTable() = default;
	private:
		std::vector<Node> tree; ///< bin tree, tree[0] … root
#ifdef DEBUG
		std::size_t d_numCodes{0};
#endif
	};

	template<std::size_t DEPTH>
	class LazyTreePreIterator {
		using address_t = HuffTable::Node::address_t;
		void push() {
			_stack[++_floor] = _next; 
		}
		void pop() {
			_next = _stack[_floor--];
		}
		void up() {
			address_t last;
			do{
				last = _next;
				pop();
				if(_next == 0 && _tree[_next].r == last) {
					_end = true;
					return;
				}
			} while(_tree[_next].l != last);
			_pos.set(_floor, true);
			push();
			_tree[_next].r = _length;
			_next = _length;
			++_length;
		}
	public:
		std::size_t size() const {
			return _length;
		}
		operator bool() {
			return !_end;
		}
		uint8_t floor() const { return _floor; }
		void set(uint8_t value) {
#ifdef DEBUG
			assert(!_end);
#endif
			HuffTable::Node& node = _tree[_next];
			node.l = 0;
			node.r = value;
			up();
		}
		LazyTreePreIterator& operator++() {
#ifdef DEBUG
			assert(!_end);
#endif
			if(_floor < DEPTH) {
				_pos.set(_floor, false);
				push();
				_tree[_next].l = _length;
				_next = _length;
				++_length;
			} else {
				up();
			}
			return *this;	
		}
		LazyTreePreIterator(HuffTable::Node*const& tree)
			: _tree{tree}{}
	private:
		std::bitset<DEPTH> _pos{0};
		bool _end{false};
		address_t _next{0};
		address_t _length{1};
		uint8_t _floor{0};
		std::array<address_t, DEPTH+1> _stack{};
		HuffTable::Node*const& _tree;
	};
}
