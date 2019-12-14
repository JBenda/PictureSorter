#include <iostream>
#include <fstream>

#include "fJPG.hpp"
#include "filesystem.hpp"
#include "Collection.hpp"
#include "GPU.hpp"
#include "Simularity.hpp"
#include "FileFormat.hpp"

int main(int argc, char *argv[])
{
	// init routine
	Collection collection{};
	for ( JpgIterator itr( "." ); itr.hastNext(); ++itr ) {
		std::ifstream file( *itr, std::ios::binary );
		fJPG::Picture pic = fJPG::Convert(file);
		file.close();
		collection << std::make_tuple(*itr, pic);
	}

	GPU::Device device{};
	collection.setBetchSize( device.batchSize() );

	Simularity simularity{};
	for ( auto itr = collection.BatchPairBegin(); itr != collection.BatchPairEnd(); ++itr ) {
		simularity.setSim(itr.first, itr.second, device.calcSim( itr.first, itr.second ));
	}
	
	FileFormat file("./out.bin");
	file.Write( simularity, collection );
	return 0;
}
