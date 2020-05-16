#include "Collection.hpp"

#include "ImageData.hpp"


Collection& operator<<( Collection& coll, ImageData&& pic ) {
	coll.add(pic);
	return coll;
}

std::ostream& operator<<( std::ostream& os, const Collection& coll ) {
	std::size_t size = coll._data.size();
	os.write(reinterpret_cast<const char*const>(&size), sizeof(size));
	return os;
}

std::istream& operator>>( std::istream& is, Collection& coll ) {
	std::size_t size;
	is.read(reinterpret_cast<char* const>(&size), sizeof(size));
	for( auto i = 0; i < size; ++i) {
		coll._data.emplace_back();
		is >> coll._data.back();
		coll.updateNewEntry(coll._data.back());
	}
	return is;
}

void Collection::updateNewEntry(const ImageData& data) {
	_totalSize += data.size();	
}

Collection::BatchPairItr& Collection::BatchPairItr::operator++() {
	++_batchNr1;
	if (_batchNr1 >= _numBatches) {
		_batchNr1 = 0;
		++ _batchNr2;
		if (_batchNr2 >= _numBatches) {
			_collection = nullptr;
		}
	}
	return *this;
}

bool Collection::BatchPairItr::operator==( const Collection::BatchPairItr& rh ) const {
	if (!_collection && !rh._collection) { return true; }
	return _collection == rh._collection
		&& _batchNr1 == rh._batchNr1
		&& _batchNr2 == rh._batchNr2;
}

bool Collection::BatchPairItr::operator!=( const Collection::BatchPairItr& rh ) const {
	return !( *this == rh);
}

std::pair<Collection::Batch, Collection::Batch> 
Collection::BatchPairItr::operator*() const {
	return {
		{	_collection->_data.begin() + _batchNr1 * _collection->_batchSize, 
			_collection->_data.begin() + (_batchNr1 + 1) * _collection->_batchSize},
		{	_collection->_data.begin() + _batchNr2 * _collection->_batchSize, 
			_collection->_data.begin() + (_batchNr2 + 1) * _collection->_batchSize}
	};
}

Collection::BatchPairItr::BatchPairItr( Collection* collection)
	: _collection{ collection }, 
		_numBatches{
			(_collection->_totalSize + _collection->_batchSize - 1)
				/ _collection->_batchSize
		} {}
