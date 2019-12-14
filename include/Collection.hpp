#pragma once

#include <tuple>
#include <filesystem>
#include <iostream>
#include "fJPG.hpp"

struct Batch {
	Batch() = default;
};

class Collection {
public:
	struct BatchPairItr;
	friend BatchPairItr;
	friend Collection& operator<<( Collection& coll, const std::tuple<const std::filesystem::path&,const fJPG::Picture<fJPG::ColorEncoding::YCrCb8>&>& pic ) {
		return coll;
	}

	Collection& operator<< ( Collection & coll ) {
	}

	friend std::ostream& operator<<( std::ostream& os, const Collection& coll ) {
		return os;
	}

	friend std::istream& operator>>( std::istream& is, Collection& coll ) {
		return is;
	}

	void setBetchSize( std::size_t x ) { _batchSize = x; }

	struct BatchPairItr {
		BatchPairItr( Collection& collection )
			: _collection{ collection } {}
		BatchPairItr& operator++() {
			return *this;
		}
		bool operator==( const BatchPairItr& rh ) const {
			return false;
		}
		bool operator!=( const BatchPairItr& rh ) const {
			return !( *this == rh);
		}
		Batch first{};
		Batch second{};
	private:
		Collection& _collection;
	};

	BatchPairItr BatchPairBegin() {
		return BatchPairItr(*this);
	}

	BatchPairItr BatchPairEnd() {
		return BatchPairItr(*this);
	}

private:
	std::size_t _batchSize{0};
};