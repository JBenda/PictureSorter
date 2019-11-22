#pragma once

#include <iosfwd>
#include <cstdint>
#include <cassert>
#include <array>

namespace fJPG {
	class HuffTable {
	public:
		/**
		 *	@brief if l == 0, r = value, and note is leaf. 
		*/
		struct Node {
			Node(uint8_t lh, uint8_t rh)
				: l{lh}, r{rh}{}
			Node() : l{0}, r{0}{}
			uint8_t l;
			uint8_t r;
		};
		
		/**
		 *	@brief decode an aligned huffman code with length <= 16
		 *	@return decoded value
		*/
		template<typename T>
		uint8_t decode(uint16_t data, T& itr) const {
			Node n = tree[0]	;
#ifdef DEBUG
			uint8_t c = 0;
#endif
			do {
				++itr;
				n = data & (0x01)
					? tree[n.r]
					: tree[n.l];
				data >>= 1;
#ifdef DEBUG
				++c;
				assert(c < 16);
#endif
			} while(n.l);
			return n.r;
		}

		friend std::istream& operator >>(std::istream& is, HuffTable& h);
	private:
		Node tree[256]; ///< bin tree, tree[0] … root
	};

	template<std::size_t DEPTH>
	class LazyTreePreIterator {
		void push() {
			_stack[++_floor] = _next; 
		}
		void pop() {
			_next = _stack[_floor--];
		}
		void up() {
			uint8_t last;
			do{
				last = _next;
				pop();
				if(_next == 0 && _tree[_next].r == last) {
					_end = true;
					return;
				}
			} while(_tree[_next].l != last);
			push();
			_tree[_next].r = _length;
			_next = _length;
			++_length;
		}
	public:
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
		bool _end{false};
		uint8_t _next{0};
		uint8_t _length{1};
		uint8_t _floor{0};
		std::array<uint8_t, DEPTH> _stack;
		HuffTable::Node*const& _tree;
	};
}
