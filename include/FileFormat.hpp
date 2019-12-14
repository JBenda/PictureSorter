#pragma once

#include "Simularity.hpp"
#include "Collection.hpp"
#include <filesystem>

class FileFormat {
public:
	FileFormat( const std::filesystem::path& path ) {}
	void Write( Simularity const& sim, Collection const& coll ) {}
};