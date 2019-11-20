#ifndef extent_server_cache_h
#define extent_server_cache_h

#include <string>
#include <map>
#include <set>
#include "extent_protocol.h"
#include "inode_manager.h"
#include "extent_server.h"

class extent_server_cache {
 private:
  inode_manager *im;

  struct fileinfo {
    std::string working_cid;
    std::set<std::string> cached_cids;
    fileinfo(){}
  };
  std::map<extent_protocol::extentid_t, fileinfo *> filemap; 
  // pthread_mutex_t extentmutex;

  void notify(extent_protocol::extentid_t id, std::string cid);
  void require(extent_protocol::extentid_t id, std::string cid);

 public:
  extent_server_cache();
  ~extent_server_cache();
  
  int create(uint32_t type,std::string cid, extent_protocol::extentid_t &id);
  int put(extent_protocol::extentid_t id, std::string cid, std::string, int &);
  int get(extent_protocol::extentid_t id, std::string cid, std::string &);
  int getattr(extent_protocol::extentid_t id, std::string cid, extent_protocol::attr &);
  int remove(extent_protocol::extentid_t id, std::string cid, int &);
};

#endif 
