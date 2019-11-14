#include "api.hpp"
#include <cassert>
#include <iostream>


int main(int argc, char *argv[])
{
	assert(("yes", add(3, 5) == 8));
	return 0;
}
