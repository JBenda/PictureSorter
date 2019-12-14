#include<filesystem>
#include<fstream>
#include<iostream>
#include <cassert>
#include <vector>
#include <cmath>

#include "fJPG.hpp"

#ifndef SAMPLE_DIR
#define SAMPLE_DIR "."
#endif

int main(int argc, char* argv[]) {
	std::string base( SAMPLE_DIR );
	std::filesystem::path pPic = base + "/t2.jpeg";
	std::ifstream img( pPic, std::ios::binary );
	
	assert( img );
	try {
		const fJPG::Picture pic = fJPG::Convert( img );
		img.close();

		std::vector<double> testData = {
			0,0,0,0,0,0,0,0,
			0,0,70,80,90,0,0,0,
			0,0,90,100,110,0,0,0,
			0,0,110,120,130,0,0,0,
			0,0,130,140,120,0,0,0,
			0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0,
			0,0,0,0,0,0,0,0
		};

		std::vector<fJPG::Imag> freq = fJPG::ToFrequenz( testData );
		std::vector<fJPG::color_t>data = fJPG::ToNormal( freq );
		
		std::ofstream out( "ori.pgm", std::ios::binary );
		unsigned int n = std::sqrt( data.size() );
		out << "P5\n" << n << ' ' << n << "\n255\n";

		auto itr = pic.GetChannel( 0 ).begin();
		for( unsigned int y = 0; y < n; ++y ) {
			for ( unsigned int x = 0; x < n; ++x ) {
				out << *itr;
				++itr;
			}
			itr += pic.GetSize().x - n;
		}
		out.close();

		out.open( "out.pgm", std::ios::binary );
		assert( n * n == data.size() );
		out << "P5\n" << n << " " << n << "\n255\n";
		for ( const fJPG::color_t c : data ) {
			out << c;
		}
		out.close();

	} catch (const std::string& msg){
		std::cerr << "conversion failed width: " << msg << '\n';
		if ( img ) img.close();
		return -1;
	}
}