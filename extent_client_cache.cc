// RPC stubs for clients to talk to extent_server

#include "extent_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include "tprintf.h"

int extent_client_cache::last_port = 0;

extent_client_cache::extent_client_cache(std::string e_dst, std::string l_dst) : extent_client(e_dst)
{
  srand(time(NULL)^last_port);
  rextent_port = ((rand()%32000) | (0x1 << 10)) + 123;
  const char *hname;
  // VERIFY(gethostname(hname, 100) == 0);
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rextent_port;
  id = host.str();
  last_port = rextent_port;
  rpcs *rerpc = new rpcs(rextent_port, 20);
  rerpc->reg(extent_protocol::pull, this, &extent_client_cache::pull_handler);
  rerpc->reg(extent_protocol::push, this, &extent_client_cache::push_handler);

	filemap[1] = new fileinfo();
	filemap[1]->attr.type = extent_protocol::T_DIR;
	// lc = new lock_client_cache(l_dst);
	// pthread_mutex_init(&extentmutex, NULL);
}

extent_client_cache::~extent_client_cache()
{
	for(std::map<extent_protocol::extentid_t, fileinfo *>::iterator it = filemap.begin(); it != filemap.end(); it ++)
		delete it->second;
}

extent_protocol::status
extent_client_cache::create(uint32_t type, extent_protocol::extentid_t &eid)
{
  extent_protocol::status ret = extent_protocol::OK;
  tprintf("\textent_client_cache: create type of %d\n", type);

  // pthread_mutex_lock(&extentmutex);
  ret = cl->call(extent_protocol::create, type, id, eid);
  VERIFY(ret == extent_protocol::OK);
	assert(filemap[eid] == NULL);
	filemap[eid] = new fileinfo();
	filemap[eid]->attr.type = type;
	filemap[eid]->attr_dirty = false;
	filemap[eid]->buf_dirty = false;
  // pthread_mutex_unlock(&extentmutex);

  return ret;
}

extent_protocol::status
extent_client_cache::get(extent_protocol::extentid_t eid, std::string &buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  tprintf("\textent_client_cache: get %lld\n", eid);
	
  // pthread_mutex_lock(&extentmutex);
	if(filemap[eid] == NULL)
		filemap[eid] = new fileinfo();
  fileinfo *info = filemap[eid];
	// queue_wait(info);
	
	// lc->acquire(eid + (1<<31));
  tprintf("\textent_client_cache: info stat %d %d %d\n", info->buf_dirty, info->attr_dirty, info->writable);
  if(info->buf_dirty){
	  tprintf("\textent_client_cache: call get\n");
  	ret = cl->call(extent_protocol::get, eid, id, buf);
		info->buf.assign(buf);
		info->buf_dirty = false;
	} else
		buf.assign(info->buf);
	// lc->release(eid + (1<<31));
	// queue_next(info);
  // pthread_mutex_unlock(&extentmutex);

  return ret;
}

extent_protocol::status
extent_client_cache::getattr(extent_protocol::extentid_t eid, 
		       extent_protocol::attr &attr)
{
  int ret = extent_protocol::OK;
  tprintf("\textent_client_cache: getattr %lld\n", eid);

  // pthread_mutex_lock(&extentmutex);
	if(filemap[eid] == NULL)
		filemap[eid] = new fileinfo();
  fileinfo *info = filemap[eid];
	// queue_wait(info);

	// lc->acquire(eid + (1<<31));
  if(info->attr_dirty){
	  tprintf("\textent_client_cache: call getattr\n");
    ret = cl->call(extent_protocol::getattr, eid, id, attr);
		info->attr = attr;
		info->attr_dirty = false;
	} else
		attr = info->attr;
	// lc->release(eid + (1<<31));
	// queue_next(info);
  // pthread_mutex_unlock(&extentmutex);

  return ret;
}

extent_protocol::status
extent_client_cache::put(extent_protocol::extentid_t eid, std::string buf)
{
  int r;
  extent_protocol::status ret = extent_protocol::OK;
  tprintf("\textent_client_cache: put %lld\n", eid);

  // pthread_mutex_lock(&extentmutex);
  fileinfo *info = filemap[eid];
  // queue_wait(info);

	// lc->acquire(eid + (1<<31));
	if(!info->writable){
	  tprintf("\textent_client_cache: call put\n");
  	ret = cl->call(extent_protocol::put, eid, id, buf, r);
 		VERIFY(ret == extent_protocol::OK);
		info->writable = true;
	}
  
	if(info){
		info->attr.atime = time(NULL);
		info->attr.mtime = time(NULL);
		info->attr.ctime = time(NULL);
		info->attr.size = buf.size();
		info->buf.assign(buf);
		info->attr_dirty = false;
		info->buf_dirty = false;
	}
	// lc->release(eid + (1<<31));
	// queue_next(info);
  // pthread_mutex_unlock(&extentmutex);

  return ret;
}

extent_protocol::status
extent_client_cache::remove(extent_protocol::extentid_t eid)
{
  int r;
  extent_protocol::status ret = extent_protocol::OK;
  tprintf("\textent_client_cache: remove %lld\n", eid);

  // pthread_mutex_lock(&extentmutex);
	// lc->acquire(eid + (1<<31));
  ret = cl->call(extent_protocol::remove, eid, id, r);
  VERIFY(ret == extent_protocol::OK);
	filemap.erase(filemap.find(eid));
	// lc->release(eid + (1<<31));
  // pthread_mutex_unlock(&extentmutex);

  return ret;
}

extent_protocol::status extent_client_cache::pull_handler(extent_protocol::extentid_t eid, int &)
{
  tprintf("\textent_client_cache: pull_handler %lld\n", eid);
	filemap[eid]->attr_dirty = true;
	filemap[eid]->buf_dirty = true;
	filemap[eid]->writable = false;
  return extent_protocol::OK;
}

extent_protocol::status extent_client_cache::push_handler(extent_protocol::extentid_t eid, int &)
{
  int r;
  extent_protocol::status ret = extent_protocol::OK;
  tprintf("\textent_client_cache: push_handler %lld\n", eid);

	ret = cl->call(extent_protocol::put, eid, id, filemap[eid]->buf, r);
 	VERIFY(ret == extent_protocol::OK);
  
	if(filemap[eid]){
		filemap[eid]->attr.atime = time(NULL);
		filemap[eid]->attr.mtime = time(NULL);
		filemap[eid]->attr.ctime = time(NULL);
		filemap[eid]->attr_dirty = false;
		filemap[eid]->buf_dirty = false;
	}
  return extent_protocol::OK;
}

void extent_client_cache::queue_wait(fileinfo *info)
{
	pthread_cond_t* thread_cond = new pthread_cond_t;
  pthread_cond_init(thread_cond, NULL);
	
	if (!info->thread_list.empty()){
    info->thread_list.push_back(thread_cond);
    pthread_cond_wait(thread_cond, &extentmutex);
  } else 
    info->thread_list.push_back(thread_cond);
}

void extent_client_cache::queue_next(fileinfo *info)
{
  delete info->thread_list.front();
	info->thread_list.pop_front();
	if (info->thread_list.size() >= 1)
    pthread_cond_signal(info->thread_list.front());
}