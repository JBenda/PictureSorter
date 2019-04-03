#include <fstream>
#include <vector>
#include <cassert>
#include <iostream>
#include <set>

template <typename T>
class CompareStore {
public:
  CompareStore(T nullValue) : _decode{nullptr}, _null{nullValue} {
  }
/*  CompareStore(std::size_t elemnets, T nullValue) : _decode{nullptr}, _null{nullValue} {
    std::size_t size = 0;
    _decode = new std::size_t[elemnets];
    while(--elemnets > 0) size += elemnets;
    _store.resize(size, _null);
  } */

  // CompareStore(std::ifstream& ifs, T nullValue) : _null{nullValue} {
  //  ifs >> *this;
  // }

  ~CompareStore(){
    if(_decode) {
      delete[] _decode;
    }
  }

  T getCompare(std::size_t id1, std::size_t id2) {
    return getEntry(id1, id2);   
  }

  T setCompare(std::size_t id1, std::size_t id2, T value) {
    getEntry(id1, id2) = value;
    return value;
  }

  friend std::ifstream& operator>>(std::ifstream& ifs, CompareStore& cs) {
    std::size_t size;
    ifs.read(reinterpret_cast<char*>(&size), sizeof(std::size_t));
    cs.inStreamResize(size);
    ifs.read(reinterpret_cast<char*>(cs._store.data()), size * sizeof(T));
    return ifs;
  }
  friend std::ofstream& operator<<(std::ofstream& ofs, CompareStore& cs) {
    std::size_t size = cs._store.size();
    ofs.write(reinterpret_cast<const char*>(&size), sizeof(std::size_t));
    ofs.write(reinterpret_cast<const char*>(cs._store.data()), size* sizeof(T));
    return ofs;
  }
  void print() {
    for(const T& e : _store)
      std::cout << e << '\n';
    std::cout << "size: " << _store.size() << "\n";
  }
  /** extend size to elemnets
   * \param elemnets new amaount of elemnets */
  void extend(std::size_t elemnets) {
    assert(_decode);
    if(_decode && _decode[0] >= elemnets) return;
    if(_decode && _decode[0] < elemnets) {
      elemnets = elemnets > _decode[0] * 1.5 ? elemnets : _decode[0] * 1.5;
      std::size_t *newDecode = new std::size_t[elemnets];
      for(std::size_t i = 1; i < _decode[0]; ++i)
        newDecode[i] = _decode[i];      
      std::size_t sum = _decode[_decode[0]-1] + _decode[0]-1;
      newDecode[0] = elemnets;
      for(std::size_t i = _decode[0]; i < elemnets; ++i) {
        newDecode[i] = sum;
        sum += i;
      }
      sum += elemnets - 1;
      delete[] _decode;
      _decode = newDecode; 
      _store.resize(sum, _null);
    }
  }
  void resize(std::size_t elemnets) {
    if(_decode && _decode[0] >= elemnets) return;
    if(_decode && _decode[0] < elemnets) {
      elemnets = elemnets > _decode[0] * 1.5 ? elemnets : _decode[0];
      delete[] _decode;
      _decode = new std::size_t[elemnets];
      _decode[0] = elemnets;
    } else if(!_decode) {
      _decode = new std::size_t[elemnets];
      _decode[0] = elemnets;
    }
    std::size_t size = 0;
    for(std::size_t i = elemnets -1; i > 0; --i) size += i;

    _store.resize(size);
    for(auto & e : _store) {
      e = _null;
    }
    std::size_t sum = 0;
    for(std::size_t i = 1; i < elemnets; ++i) {
      _decode[i] = sum;
      sum += i;
    }
  }
  
  void queueDelet(std::size_t id) {
    _deleteQueue.insert(id);
  } 
  void delet() {
    typename std::vector<T>::iterator in, out;
    in = out = _store.begin();
    if(*_deleteQueue.begin() > 0) {
      in += _decode[*_deleteQueue.begin()];
      out += _decode[*_deleteQueue.begin()];
    }
    for(auto itr = _deleteQueue.begin(); itr != _deleteQueue.end();) {
      if(*itr > 0) {
        in += _decode[*itr + 1] - _decode[*itr];
      }
      else {
        std::cout << "Itr value " << *itr << '\n';
      }
      auto oItr = itr;
      ++itr;
      for(std::size_t line = *oItr,
          pos = (line+1 == _decode[0] ? _store.size() : _decode[line + 1]) - (line ?  _decode[line] : 0); 
        line < (itr == _deleteQueue.end() ? _decode[0]  : *itr);
        --pos) {
        if(pos ==  static_cast<std::size_t>(-1)){
          ++line;
          pos = (line+1 == _decode[0] ? _store.size() : _decode[line + 1]) - _decode[line];
        } 
        if(_deleteQueue.find(pos) == _deleteQueue.end()) {// ignore entry when is for deleted Element
          if(in == _store.end()) { *out = _null; }
          else *out = *in;
          ++out;
        }
        if(in != _store.end()) ++in;
      }
    }
    _deleteQueue.clear();
  }
private:
  /** resize with length of data filed */
  void inStreamResize(std::size_t size) {
    if(_store.size() >= size) return;

    std::size_t sum = 0;
    std::size_t elemnets = 0;
    for(std::size_t i = 1; sum <= size; ++i) { sum += i; ++elemnets;}

    _store.resize(size, _null);
    if(!_decode) {
      _decode = new std::size_t[elemnets];
    }
    else if(_decode[0] < elemnets) {
      _decode[0] += 1.5;
      elemnets = elemnets > _decode[0] ? elemnets : _decode[0];
      delete[]_decode;
      _decode = new std::size_t[elemnets];
      _decode[0] = elemnets;
    }
    sum = 0;
    _decode[0] = elemnets;
    for(std::size_t i = 1; i < elemnets; ++i) {
      _decode[i] = sum;
      sum += i;
    }

  }
  T& getEntry(std::size_t id1, std::size_t id2) {
    if(id1 < id2) std::swap(id1, id2);
    assert(id1 >= id2);
    assert(_decode[id1]+(id1-id2)-1 < _store.size());
    return _store[_decode[id1] + (id1 - id2) - 1];
  }
  std::set<std::size_t> _deleteQueue;
  std::vector<T> _store;
  std::size_t* _decode;
  const T _null;
};
