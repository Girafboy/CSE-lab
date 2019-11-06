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
  thread *latest_thread = new thread();

  if (info->thread_list.empty()){
    state s = info->stat;
    info->thread_list.push_back(latest_thread);
    if (s == FREE){
      info->stat = LOCKED;
      pthread_mutex_unlock(&lockmutex);
      return lock_protocol::OK;
    } else if (s == NONE) {
      return wait_lock(info, lid, latest_thread);
    } else {
      pthread_cond_wait(&latest_thread->cv, &lockmutex);
      return wait_lock(info, lid, latest_thread);
    }
  } else {
    info->thread_list.push_back(latest_thread);
    pthread_cond_wait(&latest_thread->cv, &lockmutex);
    switch (info->stat) {
      case FREE:
        info->stat = LOCKED;
      case LOCKED:
        pthread_mutex_unlock(&lockmutex);
        return lock_protocol::OK;
      case NONE:
        return wait_lock(info, lid, latest_thread);
      default:
        assert(0);
    }
  }
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
  int r;
  int ret = rlock_protocol::OK;
  bool revoked = false;

  pthread_mutex_lock(&lockmutex);
  lockinfo *info = lockmap[lid];
  if (info->msg == REVOKE){
    revoked = true;
    info->stat = RELEASING;

    pthread_mutex_unlock(&lockmutex);
    ret = cl->call(lock_protocol::release, lid, id, r);
    pthread_mutex_lock(&lockmutex);
    info->msg = EMPTY;
    info->stat = NONE;
  } else
    info->stat = FREE;

  delete info->thread_list.front();
  info->thread_list.pop_front();
  if (info->thread_list.size() >= 1) {
    if (!revoked)
      info->stat = LOCKED;
    pthread_cond_signal(&info->thread_list.front()->cv);
  }
  pthread_mutex_unlock(&lockmutex);
  return ret;
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, 
                                  int &)
{
  int r;
  int ret = rlock_protocol::OK;
  pthread_mutex_lock(&lockmutex);
  lockinfo *info = lockmap[lid];
  if (info->stat == FREE) {
    info->stat = RELEASING;
    pthread_mutex_unlock(&lockmutex);
    ret = cl->call(lock_protocol::release, lid, id, r);
    pthread_mutex_lock(&lockmutex);
    info->stat = NONE;
    if (info->thread_list.size() >= 1)
      pthread_cond_signal(&info->thread_list.front()->cv);
  } else
    info->msg = REVOKE;
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
  info->msg = RETRY;
  pthread_cond_signal(&info->thread_list.front()->cv);
  pthread_mutex_unlock(&lockmutex);
  return ret;
}

lock_protocol::status lock_client_cache::
wait_lock(lock_client_cache::lockinfo *info,
              lock_protocol::lockid_t lid,
              thread *latest_thread) {
  int r;
  info->stat = ACQUIRING;
  while (info->stat == ACQUIRING){
    pthread_mutex_unlock(&lockmutex);
    int ret = cl->call(lock_protocol::acquire, lid, id, r);
    pthread_mutex_lock(&lockmutex);
    if (ret == lock_protocol::OK){
      info->stat = LOCKED;
      pthread_mutex_unlock(&lockmutex);
      return lock_protocol::OK;
    } else{
      if (info->msg == EMPTY) 
        pthread_cond_wait(&latest_thread->cv, &lockmutex);
      info->msg = EMPTY;
    }
  }
  assert(0);
}

lock_client_cache::~lock_client_cache() {
  pthread_mutex_lock(&lockmutex);
  for (std::map<lock_protocol::lockid_t, lockinfo *>::iterator iter = lockmap.begin(); iter != lockmap.end(); iter++)
    delete iter->second;
  pthread_mutex_unlock(&lockmutex);
}
