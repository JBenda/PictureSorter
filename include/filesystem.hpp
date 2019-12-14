#pragma once

#include <filesystem>
#include <string_view>
#include <iostream>

class JpgIterator {
	bool isJpg( const std::string_view& suffix ) {
		return
			suffix == ".jpg"
			|| suffix == ".JPG"
			|| suffix == ".jpeg"
			|| suffix == ".JPEG";
	}
	void next() {
		while ( hastNext() && ( _itr->is_directory() || !isJpg( _itr->path().extension().string() ) ) ){
			++_itr;
		}
	}
public:
	JpgIterator( const std::filesystem::path& path ) : _itr( path ), _end() { next(); }
	JpgIterator() = delete;
	JpgIterator( JpgIterator const& ) = delete;
	JpgIterator& operator=( JpgIterator const& ) = delete;
	JpgIterator( JpgIterator&& ) = delete;
	JpgIterator& operator=( JpgIterator&& ) = delete;
	~JpgIterator() = default;

	JpgIterator& operator++() {
		++_itr;
		next();
		return *this;
	}
	const std::filesystem::path& operator*() {
		return _itr->path();
	}

	bool hastNext() const {
		return _itr != _end;
	}
private:
	std::filesystem::recursive_directory_iterator _itr;
	std::filesystem::recursive_directory_iterator _end;
};