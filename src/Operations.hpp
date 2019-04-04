#pragma once

#include <filesystem>
#include <vector>
#include <fstream>
#include <iostream>
#include <cmath>
#include <cassert>
#include <unordered_map>
#include <array>
#include "CompareStore.hpp"
#include "Picture.hpp"
#include "Cluster.hpp"

#define RESOLUTION 100
// T1: id path data **Group**
// T2: **Group** name parentGroup
// T3: **Group** data
// renamePics to id-oldName.jpg 

namespace fs = std::filesystem;

struct PictureMeta {
  PictureMeta() : pos{0}, group{0}{}
  PictureMeta(std::size_t p, std::size_t g) : pos{p}, group{g}{}
  std::size_t pos;
  std::size_t group;
};

class Operations {
public:
  void printPath(int i) {
    std::cout << "path" << i << ' ' << _path << '\n';
  }
  Operations(const fs::path& path, bool isInit = false);
  ~Operations();
  /** Group of Pic or subGroups, for Hirachig sorting Pics */
  class Group {
    std::string name;
    std::size_t id;
    std::vector<Group*> childs;
    Group* parent;
  };

  /** init new Sorted Folder including all Subfolder
   * \return true on success
   */
  bool initDatabase();

  /** added new Files to existing Folder
   * \return true on success
   */
  bool updateDatabase();

  /** return array with all Group Ids
   * \return int array of all availeble GrouIds
   */
  const int* getGroups();

  /** list all Simelar Files
   * \param num GroubNumber from Group to list
   * \return array of path to all 
   */
  const std::vector<fs::path> getGroup(int num);
  const std::vector<fs::path> getGroup(const fs::path& path);

  /** delete Picture
   * \param path path from pic to delet
   * \return true on success
   */
  bool removePic(const fs::path& path);

  /** change or set GroupName
   * \param num GroupNumber
   * \param name newGroupName
   * \return true on success
   */
  bool editGroupName(int num, char* name);

  /** change sim for group to value and recalculate Groups
   * \param value new Sim Value for a group
   * \return true on success */
  void resort(double value);

  /** move all Pic like the GroupStruct
   * \param path filesStruct above this will be const 
   * \return true on success
   */
  bool groupToFolderStruct();

  /** quese file toDelete
   * \param file path to file to delte 
   * \attention files not Deleted yet, to do so call delet */
  void queueDelet(const fs::path& file);

  /** delte qued Files, and save changes 
   * \warning program crash can casue coruppted Files */
  void delet();

  /** print filePaths and check files
   * \attention in develop */
  void print();
private:
  std::vector<fs::path> _deletQueue;
  CompareStore<double> _cStore;
  const std::array<const std::string,4>  extensions = {".JPEG", ".JPG", ".jpg", ".jpeg"};
  typedef std::pair<std::string, PictureMeta> PictureMetaPath; /**< shortCut */
  double simScore(const PictureMetaPath& pPic1, const PictureMetaPath& pPic2);
  std::unordered_map<std::string, PictureMeta> _picMap;
  std::ifstream _picData;
  const fs::path _path;
  std::size_t readFile(const fs::path& path, std::vector<unsigned char>& buf);
  std::unordered_map<std::string, PictureMeta>::iterator nextIttr(const std::unordered_map<std::string, PictureMeta>::iterator& pos) {
    std::unordered_map<std::string, PictureMeta>::iterator itr = _picMap.begin();
    while(itr != pos) ++itr;
    return ++itr;
  }
  void saveMetaFile();
  void loadMetaFile();
  bool sort(double value);
  void AddPicToFile(const fs::path& path, std::ofstream& ofs);
  // calculation from simularity on cpu or gpu what is more efficent
  bool CalculateSim(size_t numElements, size_t updatesElements);
};
