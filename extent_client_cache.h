// extent client cache interface.

#ifndef extent_client_cache_h
#define extent_client_cache_h

#include <string>
#include "extent_protocol.h"
#include "extent_server.h"
#include "extent_client.h"
#include "lock_client_cache.h"

class extent_client_cache : public extent_client {
 private:
  int rextent_port;
  std::string hostname;
  std::string id;

  struct fileinfo {
    bool attr_dirty;
    bool buf_dirty;
    bool writable;
    extent_protocol::attr attr;
    std::string buf;
    std::list<pthread_cond_t *> thread_list;

    fileinfo() {
      attr_dirty = true;
      buf_dirty = true;
      writable = false;
      attr.size = 0;
      attr.type = 0;
      attr.atime = time(NULL);
      attr.mtime = time(NULL);
      attr.ctime = time(NULL);
      buf.clear();
    }
  };
  pthread_mutex_t extentmutex;
  // lock_client_cache *lc;
  std::map<extent_protocol::extentid_t, fileinfo *> filemap;

  void queue_wait(fileinfo *);
  void queue_next(fileinfo *);
 public:
  static int last_port;
  extent_client_cache(std::string e_dst, std::string l_dst);
  ~extent_client_cache();
  extent_protocol::status create(uint32_t type, extent_protocol::extentid_t &eid);
  extent_protocol::status get(extent_protocol::extentid_t eid, 
			                        std::string &buf);
  extent_protocol::status getattr(extent_protocol::extentid_t eid, 
				                          extent_protocol::attr &a);
  extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);
  extent_protocol::status remove(extent_protocol::extentid_t eid);

  extent_protocol::status pull_handler(extent_protocol::extentid_t eid, int &);
  extent_protocol::status push_handler(extent_protocol::extentid_t eid, int &);
};

#endif 

