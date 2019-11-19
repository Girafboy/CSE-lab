// extent client cache interface.

#ifndef extent_client_cache_h
#define extent_client_cache_h

#include <string>
#include "extent_protocol.h"
#include "extent_server.h"
#include "extent_client.h"

class extent_client_cache : public extent_client {
 private:
  int rextent_port;
  std::string hostname;
  std::string id;

  struct fileinfo {
    bool attr_dirty;
    bool buf_dirty;
    extent_protocol::attr attr;
    std::string buf;

    fileinfo() {
      attr_dirty = true;
      buf_dirty = true;
      attr.size = 0;
      attr.type = 0;
      attr.atime = time(NULL);
      attr.mtime = time(NULL);
      attr.ctime = time(NULL);
      buf.clear();
    }
  };

  std::map<extent_protocol::extentid_t, fileinfo *> filemap;

 public:
  static int last_port;
  extent_client_cache(std::string dst);
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

