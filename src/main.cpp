#include "Picture.hpp"
#include "Cluster.hpp"
#include "Operations.hpp"
#include <filesystem>

namespace fs = std::filesystem;

int main(int argn, char** argv) {
	Operations opt(fs::path(std::string(".")));
	if (argn < 2) {
		std::cout << "no so\n";
		return 1;
	}
	std::string arg1(argv[1]);
	if (argn == 2) {
		if (!arg1.compare("--help")) {
			std::cout << "init\t:init System\nupdate\t:updtae folder\nsim [fileName]\t:search al sim pictures";
			return 0;
		}
		else if (!arg1.compare("list")) {
			opt.print();
		}
		else if (!arg1.compare("init")) {
			opt.CalculateSim(4, 4);
			// opt.initDatabase();
			return 0;
		}
		else if (!arg1.compare("update")) {
			opt.updateDatabase();
			return 0;
		}
		else if (!arg1.compare("order")) {
			opt.groupToFolderStruct();
		}
	}
	else if (argn == 3) {
		if (!arg1.compare("sim")) {
			for (const fs::path& p : opt.getGroup(fs::path(std::string(argv[2])))) {
				std::cout << p << '\n';
			}
		}
		else if (!arg1.compare("reSort")) {
			opt.printPath(6);
			double value = -1.;
			sscanf(argv[2], "%lf", &value);
			opt.printPath(7);
			if (value >= .0)
				opt.resort(value);
			else {
				std::cout << "enter Sim Value for next Sort\n";
				return 0;
			}
		}
		else if (!arg1.compare("del")) {
			opt.queueDelet(fs::relative(fs::path(argv[2])));
			opt.delet();
		}
	}
}
/* int main(void) {
	std::cout << "Filename: ";
	std::string filename;
	std::cin >> filename;
	fs::path p(filename);
	if (!fs::exists(p)) {
		std::cerr << "file not found " << filename << "\n";
		return 0;
	}
	std::ifstream picture(p, std::ios::binary);
	if (picture.fail() || !picture.good()) {
		std::cout << "failed to open file\n";
		return 0;
	}
	int len = fs::file_size(p);
	std::vector<char> fileData(len);
	picture.read(fileData.data(), len);
	Image img(std::move(fileData));
	if (!img.Parse()) {
		std::cerr << "Failed to parse\n";
		img.PrintError();
		return 0;
	}
	img.PrintInfo();
	return 0;
	// Create the two input vectors
} */