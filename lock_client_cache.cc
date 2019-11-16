// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"
#include <unistd.h>

int lock_client_cache::last_port = 0;

lock_client_cache::lock_client_cache(std::string xdst, 
				     class lock_release_user *_lu)
  : lock_client(xdst), lu(_lu)
{
  srand(time(NULL)^last_port);
  rlock_port = ((rand()%32000) | (0x1 << 10));
  const char *hname;
  // VERIFY(gethostname(hname, 100) == 0);
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlock_port;
  id = host.str();
  last_port = rlock_port;
  rpcs *rlsrpc = new rpcs(rlock_port);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);

  pthread_mutex_init(&lockmutex, NULL);
}

lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
  pthread_mutex_lock(&lockmutex);
  if (lockmap.find(lid) == lockmap.end())
    lockmap[lid] = new lockinfo();
  lockinfo *info = lockmap[lid];
  pthread_cond_t* thread_cond = new pthread_cond_t;
  pthread_cond_init(thread_cond, NULL);

  // fairy
  if (!info->thread_list.empty()){
    info->thread_list.push_back(thread_cond);
    pthread_cond_wait(thread_cond, &lockmutex);
  } else 
    info->thread_list.push_back(thread_cond);

  while(true)
    switch(info->stat){
      case lockinfo::NONE:
        while (true){
          int ret = call_acquire(info, lid, thread_cond);
          if (ret == lock_protocol::OK){
            info->stat = lockinfo::LOCKED;
            pthread_mutex_unlock(&lockmutex);
            return lock_protocol::OK;
          } else{
            if (info->msg != lockinfo::RETRY) 
              pthread_cond_wait(thread_cond, &lockmutex);
            info->msg = lockinfo::EMPTY;
          }
        }
      case lockinfo::FREE:
        info->stat = lockinfo::LOCKED;
        pthread_mutex_unlock(&lockmutex);
        return lock_protocol::OK;
      default:
        pthread_cond_wait(thread_cond, &lockmutex);    
    }
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
  int ret = rlock_protocol::OK;

  pthread_mutex_lock(&lockmutex);
  lockinfo *info = lockmap[lid];
  if (info->msg == lockinfo::REVOKE){
    ret = call_release(info, lid);
    assert(ret == rlock_protocol::OK);
    info->msg = lockinfo::EMPTY;
    info->stat = lockinfo::NONE;
  } else
    info->stat = lockinfo::FREE;

  delete info->thread_list.front();
  info->thread_list.pop_front();
  if (info->thread_list.size() >= 1)
    pthread_cond_signal(info->thread_list.front());
  pthread_mutex_unlock(&lockmutex);
  return ret;
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, 
                                  int &)
{
  int ret = rlock_protocol::OK;
  pthread_mutex_lock(&lockmutex);
  lockinfo *info = lockmap[lid];
  if (info->stat == lockinfo::FREE) {
    ret = call_release(info, lid);
    info->stat = lockinfo::NONE;
    if (info->thread_list.size() >= 1)
      pthread_cond_signal(info->thread_list.front());
  } else
    info->msg = lockinfo::REVOKE;
  pthread_mutex_unlock(&lockmutex);
  return ret;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, 
                                 int &)
{
  int ret = rlock_protocol::OK;
  pthread_mutex_lock(&lockmutex);
  lockinfo *info = lockmap[lid];
  info->msg = lockinfo::RETRY;
  pthread_cond_signal(info->thread_list.front());
  pthread_mutex_unlock(&lockmutex);
  return ret;
}

lock_protocol::status lock_client_cache::
call_acquire(lock_client_cache::lockinfo *info,
              lock_protocol::lockid_t lid,
              pthread_cond_t *thread_cond) {
  int r;
  info->stat = lockinfo::ACQUIRING;
  pthread_mutex_unlock(&lockmutex);
  int ret = cl->call(lock_protocol::acquire, lid, id, r);
  pthread_mutex_lock(&lockmutex);
  return ret;
}

lock_protocol::status lock_client_cache::call_release(lock_client_cache::lockinfo *info,lock_protocol::lockid_t lid) {
  int r;
  info->stat = lockinfo::RELEASING;
  pthread_mutex_unlock(&lockmutex);
  int ret = cl->call(lock_protocol::release, lid, id, r);
  pthread_mutex_lock(&lockmutex);
  return ret;
}

lock_client_cache::~lock_client_cache() {
  pthread_mutex_lock(&lockmutex);
  for (std::map<lock_protocol::lockid_t, lockinfo *>::iterator iter = lockmap.begin(); iter != lockmap.end(); iter++)
    delete iter->second;
  pthread_mutex_unlock(&lockmutex);
}
