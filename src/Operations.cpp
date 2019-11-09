#include "Operations.hpp"
#include <iostream>
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstdio>

class Pos {
public:
  Pos(std::size_t xe, std::size_t ye, std::size_t w, std::size_t h)
    : x{xe},
      y{ye},
      pos{y*w+x},
      width{w},
      height{h}
  {}
  Pos& operator++() {
    if(++x >= width) { x = 0; ++y;}
    ++pos;
    return *this;
  }
  Pos& operator--() {
    if(x == 0) {
      x = width;
      --y;
    }
    --x;
    --pos;
    return *this;
  }
  /** if the position is in scope
   * return bool as indicator */
  bool isIn()const {
    return y < height;
  }
  std::size_t getX()const { return x; }
  std::size_t getY()const { return y; }
  /** get px number  */
  std::size_t getPos() { return pos; }
private:
  std::size_t x, y, pos;
  const std::size_t width, height;
};

double Operations::simScore(const PictureMetaPath& pPic1, const PictureMetaPath& pPic2) {
  if(double sim = _cStore.getCompare(pPic1.second.pos, pPic2.second.pos); sim != .0) {
    return sim;
  }
  assert(false);
  return 0;
}

/** read File raw Data in char vector
 * \return size of File/readed datab*/
std::size_t Operations::readFile(const fs::path& path, std::vector<unsigned char>& buf) {
  FILE* f = fopen(path.string().c_str(), "rb");
  if (!f) {
      printf("Error opening the input file.\n");
      return false;
  }
  fseek(f, 0, SEEK_END);
  std::size_t size = ftell(f);
  if( buf.size() < size)  {
    if (buf.size() * 1.5f > size)
      buf.resize(buf.size() * 1.5);
    else
      buf.resize(size);
  }
  fseek(f, 0, SEEK_SET);

  if (fread(buf.data(), 1, size, f) != size)
  {
    std::cout << "Not read correct\n";
    return false;
  }
  fclose(f);
  return size;
}

void Operations::AddPicToFile(const fs::path& path, std::ofstream& ofs) {
	assert(fs::exists(path));
	size_t len = fs::file_size(path);
	std::vector<char> data(len);
	std::ifstream file(path, std::ios::binary);
	file.read(data.data(), len);
	Image image(std::move(data));
	image.Parse();
	ofs << image;
}

bool Operations::initDatabase()
{
  std::vector<fs::path> pathes;
  for(auto &p: fs::recursive_directory_iterator(_path))
  {
    if (fs::is_regular_file(p))
    {
      if(std::find(extensions.begin(), extensions.end(), p.path().extension().string()) != extensions.end()){
        pathes.push_back(fs::relative(p.path()));
      }
    }
  }
  std::cout << pathes.size() << " files Found ->" << (Image::memorySize * pathes.size()) << "KB \n";

  
  _picMap.reserve(pathes.size());
  
  std::size_t i = 0;
  std::ofstream picDataFile((_path / "Pic.dat").string(),std::ofstream::binary);

  std::vector<unsigned char> buf;
  std::size_t size;

  for(const fs::path& p : pathes) {
    std::cout << p.string() << '\n';
    _picMap.insert(PictureMetaPath(p.string(), PictureMeta(i, 0)));
	AddPicToFile(p, picDataFile);
    i++;

  }

  std::cout << "fin meta calc\n";

  picDataFile.close();
  
  saveMetaFile();


  CalculateSim(_picMap.size(), _picMap.size());

  sort(.7);

  saveMetaFile();
  return true;
}

void Operations::resort(double value) {
  sort(value);
  saveMetaFile();
  for(PictureMetaPath pmp : _picMap) {
    std::cout << pmp.first << " " << pmp.second.group << '\n';
  }
}

bool Operations::sort(double value) {
  std::size_t groupCount = 0;

  std::unordered_map<std::string, PictureMeta>::iterator pic1, pic2;
  _picData.open((_path / "Pic.dat").string(), std::ifstream::binary);
  // init groupValue
  for(pic1 = _picMap.begin(); pic1 != _picMap.end(); ++pic1) {
    pic1->second.group = 0;
  }
  _cStore.resize(_picMap.size());
  

  for(pic1 = _picMap.begin(); pic1 != _picMap.end(); ++pic1) {
    for(pic2 = nextIttr(pic1); pic2 != _picMap.end(); ++pic2) {
      // if(pic2 == _picMap.end()) break; TODO remove
      if (simScore(*pic1, *pic2) > value) { // TODO: set flex Value
        if(pic1->second.group || pic2->second.group) {  // one is in Group
          if(pic1->second.group && pic2->second.group) {  // mereg groups 
            if(pic1->second.group != pic2->second.group) {
              return false; 
            } 
          } else if (pic1->second.group) {
            pic2->second.group = pic1->second.group;
          } else {
            pic1->second.group = pic2->second.group;
          }
        } else {
          pic1->second.group = pic2->second.group = ++groupCount;
        }
      }
    }
  }
  _picData.close();
  return true;
}
// MetaFile
//  * HashMap num Elemnt
//  []:
//    * length path
//    * path
//    * metaData

// PicFile
// []: PicData

const std::vector<fs::path> Operations::getGroup(const fs::path& path) {
  std::size_t group = 0, pos;
  std::vector<fs::path> vec;
  std::cout << fs::relative(path) << '\n';
  const std::unordered_map<std::string, PictureMeta>::iterator pair = _picMap.find(fs::relative(path).string());
  if(pair != _picMap.end()) {
     group = pair->second.group;
     pos = pair->second.pos;
  }
  if(!group) {
    std::cout << "no sim pics\n";
    return vec;
  }
  for(const PictureMetaPath& p : _picMap) {
    if(p.second.group == group && p.second.pos != pos) {
      vec.push_back(p.first);
    }
  }
  return vec;
}

bool Operations::updateDatabase(){
  std::vector<unsigned char> buf;
  std::vector<fs::path> newPathes;
  // remove old components
  for(const auto& p : _picMap) {
    if(!fs::exists(p.first)){
      queueDelet(p.first);
    }
  }
  delet();

  // add new components
  for(const auto &p: fs::recursive_directory_iterator(_path))
  {
    if (fs::is_regular_file(p))
    {
      if(std::find(extensions.begin(), extensions.end(), p.path().extension().string()) != extensions.end()){
        // TODO: store result ?
		if (const auto ptr = _picMap.find(fs::relative(p.path()).string()); ptr == _picMap.end()) {
			newPathes.push_back(fs::relative(p.path()));
		}
      }
    }
  }
 

  for(const auto &p : newPathes)
    std::cout << p << '\n';
  
  _cStore.extend(_picMap.size() + newPathes.size());
  std::ofstream picDataFile((_path / "Pic.dat").string(), std::ofstream::binary | std::ofstream::app);
  std::size_t posN = _picMap.size();
  std::size_t size;
  std::size_t groupNum = 0;
  for(const auto &e : _picMap) {
    if(e.second.group > groupNum) {
      groupNum = e.second.group;
    }
  }
  _picData.open((_path / "Pic.dat").string(), std::ifstream::binary);
  for (const auto &p : newPathes) {
	  AddPicToFile(p, picDataFile);
  }

  CalculateSim(_picMap.size() + newPathes.size(), newPathes.size());
  
  for(const auto &p : newPathes) {
    PictureMetaPath entry(p.string(), PictureMeta(posN, 0));
    for(auto &mapE : _picMap) { 
      if(simScore(entry, mapE) > .7) { // TODO: set flex Value
        if(mapE.second.group) {
          entry.second.group = mapE.second.group;
        } else {
          ++groupNum;
          entry.second.group = groupNum;
          mapE.second.group = groupNum;
        }
      }
    }
    ++posN;
    _picMap.insert(entry);
  }
  picDataFile.close();
  _picData.close();
  saveMetaFile();

  return false;
}
void Operations::saveMetaFile(){
  std::ofstream picMetaFile((_path / "PicMeta.dat").string());
  std::cout << "save" << (_path / "PicMeta.dat") << '\n';
  std::size_t size = _picMap.size();
  picMetaFile.write(reinterpret_cast<const char*>(&size), sizeof(std::size_t));
  for(const PictureMetaPath& set : _picMap){
    size = set.first.size();
    picMetaFile.write(reinterpret_cast<const char*>(&size), sizeof(std::size_t));
    picMetaFile.write(set.first.c_str(), size);
    picMetaFile.write(reinterpret_cast<const char*>(&set.second), sizeof(PictureMeta));
  }
  // match matrix
  picMetaFile << _cStore;
  picMetaFile.close();
}
void Operations::loadMetaFile(){
  // path id/group Map
  std::ifstream picMetaFile((_path / "PicMeta.dat").string(), std::ifstream::binary);
  std::size_t size;
  std::size_t pathSize;
  picMetaFile.read(reinterpret_cast<char*>(&size), sizeof(std::size_t));
  _picMap.reserve(size);
  std::string str;
  PictureMeta pM;
  for(std::size_t i = 0; i < size; ++i) {
    picMetaFile.read(reinterpret_cast<char*>(&pathSize), sizeof(std::size_t));
    str.resize(pathSize);
    picMetaFile.read(reinterpret_cast<char*>(str.data()), pathSize);
    picMetaFile.read(reinterpret_cast<char*>(&pM), sizeof(PictureMeta));
    _picMap.insert(PictureMetaPath(str,pM));
  }
  // match matrix
  picMetaFile >> _cStore;
  picMetaFile.close();
}

bool Operations::groupToFolderStruct() {
  fs::path folderPath = _path / ".." / "orders";
  if(fs::exists(folderPath)) {
    fs::remove_all(folderPath);
  }
  fs::create_directory(folderPath);
  std::size_t groupCount = 0;
  char num[5];
  for(const PictureMetaPath& p : _picMap) {
    while(groupCount <= p.second.group) {
      sprintf(num, "%zu", groupCount);
      fs::create_directory(folderPath / num);
      ++groupCount;
    }
    fs::path fPath(p.first);
    sprintf(num, "%zu", p.second.group);
    fs::path nPath = folderPath / num / fPath.filename();
    fs:: create_hard_link(fPath, nPath);
  }
  return true;
}

Operations::Operations(const fs::path& path, bool isInit) : _cStore{.0}, _path{path} {
  if(fs::exists(_path / "PicMeta.dat"))
    isInit = true;
  if(isInit) {
    loadMetaFile();
  }
}
Operations::~Operations() {}

void Operations::queueDelet(const fs::path& file) {
  _deletQueue.push_back(file);
}

void Operations::delet() {
  if(_deletQueue.size() == 0) return;
  std::size_t size = _picMap.size();
  std::vector<std::size_t> newPos(size, 0);
  for(const fs::path& p : _deletQueue) {
    std::cout << p << '\n';
    std::unordered_map<std::string, PictureMeta>::const_iterator itr = _picMap.find(p.string());
    std::cout << "itr: " << itr->first << '\n';
    if(itr != _picMap.end()) {
      std::cout << "call\n";
      newPos[itr->second.pos] = size;
      _cStore.queueDelet(itr->second.pos);
      _picMap.erase(itr);
    }
  }
  _cStore.delet();

  // calculate new Positions
  // remove pictureData
  char buffer[Image::memorySize];
  std::ifstream ifs(_path / "Pic.dat", std::ifstream::binary);
  std::ofstream ofs(_path / "nPic.dat", std::ofstream::binary);
  std::size_t pos = 0;
  for(std::size_t& p : newPos) {
    if(p < size) {
      ifs.read(buffer, Image::memorySize);
      ofs.write(buffer, Image::memorySize);
      p = pos;
      ++pos;
    } else {
      ifs.seekg(static_cast<std::size_t>(ifs.tellg()) + Image::memorySize, ifs.beg);
    }
  }
  ifs.close();
  ofs.close();

  // fetch new positions
  for(auto& p: _picMap) {
    p.second.pos = newPos[p.second.pos];
  }
  saveMetaFile();
  _deletQueue.resize(0);
  fs::remove(_path / "Pic.dat");
  fs::rename(_path / "nPic.dat", _path / "Pic.dat");
}

void Operations::print() {
  std::size_t var[3];
  std::ifstream ifs((_path / "Pic.dat").string(), std::ifstream::binary);
  if(!ifs) {
    std::cout << "cant open file\n";
    return;
  }
  ifs.seekg(0, ifs.end);
  std::size_t fileSize = ifs.tellg();
  std::cout << fileSize << "\n";

  if(fileSize % Image::memorySize != 0) {
    std::cout << "corrupted data File, no valid length\n";
  }
  std::size_t el = fileSize / Image::memorySize;
  std::cout << el << "  el " << _picMap.size() << '\n';
  if(el != _picMap.size()) {
    if(_picMap.size() > el) {
      std::cout << "daten file is too small.\n";
    } else {
      std::cout << "daten file is too large.\n";
    }
    return;
  }
 
  ifs.seekg(0, ifs.beg);
  for(const PictureMetaPath& p : _picMap) {
    if(p.second.pos >= _picMap.size()) {
      std::cout << "invalid pos\n";
      return;
    }
    ifs.seekg(Image::memorySize * p.second.pos, ifs.beg);
    ifs.read(reinterpret_cast<char*>(var), sizeof(var));
    std::cout << p.first << '\t'<< p.second.pos << '\t' << p.second.group << '\t';
    std::cout << '(' << var[0] << ", " << var[1] << ", " << var[2] << ")\n";
  }
  ifs.close();
}

bool Operations::CalculateSim(size_t numElements, size_t updatesElements) {
	_picData.open((_path / "Pic.dat").string(), std::ifstream::binary);
	size_t size = fs::file_size((_path / "Pic.dat"));
	std::vector<float> buff(size);
	_picData.read(reinterpret_cast<char*>(buff.data()), size);
	_picData.close();
        _cStore.resize(numElements);

	GPUMagic::CalculateSim(buff.data(), Image::dimX * Image::dimY, size / (sizeof(float) * Image::dimX * Image::dimY), _cStore.raw());

        _cStore.print();
        for(int y = 0; y < numElements; ++y) {
            for(int x = 0; x < numElements; ++x) {
                if(x != y) {
                    std::cout << x << ":" << y << "\t" << _cStore.getCompare(y,x) << '\n';
                }
            }
        }

	return true;
}
/*
int main(int argn, char** argv) {
  Operations opt(fs::path(std::string(".")));
  if(argn < 2) {
    std::cout << "no so\n";
    return 1;
  }
  std::string arg1(argv[1]);
  if(argn == 2) {
    if(!arg1.compare("--help")) {
      std::cout << "init\t:init System\nupdate\t:updtae folder\nsim [fileName]\t:search al sim pictures";
      return 0;
    }
    else if(!arg1.compare("list")) {
      opt.print();
    }
    else if(!arg1.compare("init")) {
      opt.initDatabase(); 
      return 0;
    }
    else if(!arg1.compare("update")) {
      opt.updateDatabase();
      return 0;
    } else if(!arg1.compare("order")) {
      opt.groupToFolderStruct();
    }
  } else if(argn == 3) {
    if(!arg1.compare("sim")) {
      for(const fs::path& p: opt.getGroup(fs::path(std::string(argv[2])))) {
        std::cout << p << '\n';
      }
    } else if (!arg1.compare("reSort")) {
      opt.printPath(6);
      double value = -1.;
      sscanf(argv[2], "%lf", &value);
      opt.printPath(7);
      if(value >= .0)
        opt.resort(value);
      else {
        std::cout << "enter Sim Value for next Sort\n";
        return 0;
      }
    } else if(!arg1.compare("del")) {
      opt.queueDelet(fs::relative(fs::path(argv[2])));
      opt.delet();
    }
  }
} */
