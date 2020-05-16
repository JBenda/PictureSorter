#include "fJPG.hpp"
#include "filesystem.hpp"
#include "Collection.hpp"
#include "GPU.hpp"
#include "Simularity.hpp"
#include "FileFormat.hpp"
#include "ImageData.hpp"

#include <iostream>
#include <fstream>
#include <cassert>

int main(int argc, char *argv[])
{
	// init routine
	Collection collection{};
	for ( JpgIterator itr( "." ); itr.hastNext(); ++itr ) {
		std::cout << *itr << " file\n";
		std::ifstream file( *itr, std::ios::binary );
		fJPG::Picture pic = fJPG::Convert(file);
		file.close();
        assert(pic.GetSize().x * pic.GetSize().y == pic.GetChannel(0).size());
        collection << ImageData(
            *itr,
            pic.GetChannel(0).data(),
            pic.GetSize().x,
            pic.GetSize().y);
	}

	// GPU::Device device{};
	// collection.setBetchSize( device.batchSize() );

	Simularity simularity{};

	for ( const auto& img : collection) {
		simularity.addImage(img);
	}

	FileFormat file("./out.bin");
	file.Write( simularity, collection );
	return 0;
}
