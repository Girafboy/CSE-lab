#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>


#include <map>
#include <set>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"


class lock_server_cache {
 private:
  int nacquire;
  enum state
  {
    NONE,
    LOCKED,
    REVOKING,
    RETRYING
  };

  struct lockinfo
  {
    std::string owner;
    std::string retrying_cid;
    std::set<std::string> waiting_cids;
    state stat;
  };

  std::map<lock_protocol::lockid_t, lockinfo *> lockmap;
  pthread_mutex_t lockmutex;

 public:
  lock_server_cache();
  ~lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);
};

#endif
