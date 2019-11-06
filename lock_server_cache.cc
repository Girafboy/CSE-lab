// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"


lock_server_cache::lock_server_cache()
{
  VERIFY(pthread_mutex_init(&lockmutex, NULL) == 0);
}


int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, 
                               int &)
{
  int r;
  lock_protocol::status ret = lock_protocol::OK;
  pthread_mutex_lock(&lockmutex);

  if (lockmap.find(lid) == lockmap.end()){
    lockmap[lid] = new lockinfo();
    lockmap[lid]->stat = NONE;
  }

  lockinfo *info = lockmap[lid];

  switch (info->stat){
    case NONE:
      assert(info->owner.empty());
      info->stat = LOCKED;
      info->owner = id;
      pthread_mutex_unlock(&lockmutex);
      break;

    case LOCKED:
      assert(!info->waiting_cids.count(id));
      assert(!info->owner.empty());
      assert(info->owner != id);
      info->waiting_cids.insert(id);
      info->stat = REVOKING;
      pthread_mutex_unlock(&lockmutex);
      handle(info->owner).safebind()->call(rlock_protocol::revoke, lid, r);
      ret = lock_protocol::RETRY;
      break;

    case REVOKING:
      assert(!info->waiting_cids.count(id));
      info->waiting_cids.insert(id);
      pthread_mutex_unlock(&lockmutex);
      ret = lock_protocol::RETRY;
      break;

    case RETRYING:
      if (id == info->retrying_cid){
        assert(!info->waiting_cids.count(id));
        info->retrying_cid.clear();
        info->stat = LOCKED;
        info->owner = id;
        if (!info->waiting_cids.empty()){
          info->stat = REVOKING;
          pthread_mutex_unlock(&lockmutex);
          handle(id).safebind()->call(rlock_protocol::revoke, lid, r);
        } else 
          pthread_mutex_unlock(&lockmutex);
      } else {
        assert(!info->waiting_cids.count(id));
        info->waiting_cids.insert(id);
        pthread_mutex_unlock(&lockmutex);
        ret = lock_protocol::RETRY;
      }
      break;
      
    default:
      assert(0);
  }
  return ret;
}

int 
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, 
         int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  pthread_mutex_lock(&lockmutex);
  lockinfo *info = lockmap[lid];
  assert(info);
  assert(info->stat == REVOKING);
  assert(info->owner == id);
  std::string nextid = *(info->waiting_cids.begin());
  info->waiting_cids.erase(info->waiting_cids.begin());
  info->retrying_cid = nextid;
  info->owner.clear();
  info->stat = RETRYING;
  pthread_mutex_unlock(&lockmutex);
  handle(nextid).safebind()->call(rlock_protocol::retry, lid, r);
  return ret;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  tprintf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}

lock_server_cache::~lock_server_cache() {

  pthread_mutex_lock(&lockmutex);
  for (std::map<lock_protocol::lockid_t, lockinfo *>::iterator iter = lockmap.begin(); iter != lockmap.end(); iter++)
    delete iter->second;

  pthread_mutex_unlock(&lockmutex);
}
