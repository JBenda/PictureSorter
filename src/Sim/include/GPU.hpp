#pragma once

#include "Collection.hpp"

#include <cstddef>
#include <vector>

namespace GPU {
	class Device {
	public:
		std::size_t batchSize() { return 1; }
		std::vector<float> calcSim( const Collection::Batch& b1, const Collection::Batch& b2 ) const {
			return std::vector<float>();
		}
	};
}
