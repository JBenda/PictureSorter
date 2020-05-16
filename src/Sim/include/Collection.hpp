#pragma once

#include "Settings.hpp"

#include <cassert>
#include <tuple>
#include <filesystem>
#include <iosfwd>
#include <vector>

struct ImageData;

/**
 * @brief collection of image metadata.
 */
class Collection {
public:
	struct BatchPairItr;
	friend BatchPairItr;

	/**
	 * @brief add picture to collection 
	 * @param coll target collection
	 * @param pic picture to add
	 */
	friend Collection& operator<<( Collection& coll, ImageData&& pic );

	/**
	 * @brief serialize a collection 
	 * @param os output stream
	 * @param coll collection to serialize
	 */
	friend std::ostream& operator<<( std::ostream& os, const Collection& coll );

	/**
	 * @brief de-serialize a collection 
	 * TODO: how handle redundant images?
	 * @param is input stream
	 * @param coll collection to add metadata
	 */
	friend std::istream& operator>>( std::istream& is, Collection& coll );


private:
	void updateNewEntry(const ImageData& data);

	void add(const ImageData& data) { 
		_data.push_back(data); 
		updateNewEntry(_data.back());
	}

	void add(ImageData&& data) { 
		_data.push_back(data);
		updateNewEntry(_data.back());
	}
	std::vector<ImageData> _data;
	std::size_t _batchSize{1};		///< batch size in Byte 
	std::size_t _totalSize{0};    ///< total size of collection data in Byte
	
public:
	using iterator = decltype(_data)::iterator;
	using const_iterator = decltype(_data)::const_iterator;
	using Batch = std::pair<Collection::iterator, Collection::iterator>;

	
	const_iterator begin() { return _data.cbegin(); }
	const_iterator end() { return _data.cbegin(); }
	
	/// Set batch size, for batch iterator (x in Byte)
	void setBetchSize( std::size_t x ) { assert(x > 0); _batchSize = x; }

	/// Iterate through all pairs of batches for comparison.
	/// @attention will get invalid if collection changes!! 
	struct BatchPairItr {
		friend Collection;
		BatchPairItr& operator++();

		bool operator==( const BatchPairItr& rh ) const;

		bool operator!=( const BatchPairItr& rh ) const;

		std::pair<Batch, Batch> operator*() const;

	private:

		BatchPairItr() = default; 
		BatchPairItr( Collection* collection);

		Collection*  _collection{nullptr};
		const std::size_t _numBatches{0};	///< total number of batches in collection 
		std::size_t _batchNr1{0};
		std::size_t _batchNr2{0};
	};

	/// get begin iterator for batch pairs
	BatchPairItr BatchPairBegin() {
		return BatchPairItr(this);
	}

	/// get end iterator for batch pairs
	BatchPairItr BatchPairEnd() {
		return BatchPairItr{};
	}

};
