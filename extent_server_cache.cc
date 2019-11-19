// the extent server implementation

#include "extent_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "handle.h"

extent_server_cache::extent_server_cache() 
{
  im = new inode_manager();
	filemap[1] = new fileinfo();
}

extent_server_cache::~extent_server_cache()
{
	for(std::map<extent_protocol::extentid_t, fileinfo *>::iterator it = filemap.begin(); it != filemap.end(); it++)
		delete it->second;
}

int extent_server_cache::create(uint32_t type, std::string cid, extent_protocol::extentid_t &id)
{
  // alloc a new inode and return inum
  printf("extent_server_cache: create inode type %d\n", type);
  id = im->alloc_inode(type);
	
	assert(filemap[id] == NULL);
	filemap[id] = new fileinfo();
	filemap[id]->cached_cids.insert(cid);
  return extent_protocol::OK;
}

int extent_server_cache::put(extent_protocol::extentid_t id, std::string cid, std::string buf, int &)
{
  printf("extent_server_cache: put %lld\n", id);
  id &= 0x7fffffff;
  
  const char * cbuf = buf.c_str();
  int size = buf.size();
  im->write_file(id, cbuf, size);
  
	assert(filemap[id]);
	filemap[id]->cached_cids.insert(cid);
	notify(id, cid);
  return extent_protocol::OK;
}

int extent_server_cache::get(extent_protocol::extentid_t id, std::string cid, std::string &buf)
{
  printf("extent_server_cache: get %lld\n", id);

  id &= 0x7fffffff;

  int size = 0;
  char *cbuf = NULL;

  im->read_file(id, &cbuf, &size);
  if (size == 0)
    buf = "";
  else {
    buf.assign(cbuf, size);
    free(cbuf);
  }

	assert(filemap[id]);
	filemap[id]->cached_cids.insert(cid);
  return extent_protocol::OK;
}

int extent_server_cache::getattr(extent_protocol::extentid_t id, std::string cid, extent_protocol::attr &a)
{
  printf("extent_server_cache: getattr %lld\n", id);

  id &= 0x7fffffff;
  
  extent_protocol::attr attr;
  memset(&attr, 0, sizeof(attr));
  im->getattr(id, attr);
  a = attr;

	assert(filemap[id]);
	filemap[id]->cached_cids.insert(cid);
  return extent_protocol::OK;
}

int extent_server_cache::remove(extent_protocol::extentid_t id, std::string cid, int &)
{
  printf("extent_server_cache: remove %lld\n", id);

  id &= 0x7fffffff;
  im->remove_file(id);

	notify(id, cid);
  return extent_protocol::OK;
}

void extent_server_cache::notify(extent_protocol::extentid_t id, std::string cid)
{
	int r;
  printf("extent_server_cache: notify %lld\n", id);

	if(!filemap[id])
		return;
	if(filemap[id]->cached_cids.empty())
		return;

	for(std::set<std::string>::iterator it = filemap[id]->cached_cids.begin(); it != filemap[id]->cached_cids.end(); it ++)
		if(cid != *it){
			printf("\taccepter: %s\n", it->c_str());
			handle(*it).safebind()->call(extent_protocol::pull, id, r);
		}
}
