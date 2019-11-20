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
#include "tprintf.h"

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
  tprintf("extent_server_cache: create inode type %d from %s\n", type, cid.c_str());
  id = im->alloc_inode(type);
	
	assert(filemap[id] == NULL);
	filemap[id] = new fileinfo();
	filemap[id]->cached_cids.insert(cid);
  return extent_protocol::OK;
}

int extent_server_cache::put(extent_protocol::extentid_t id, std::string cid, std::string buf, int &)
{
  tprintf("extent_server_cache: put %lld from %s\n", id, cid.c_str());
  id &= 0x7fffffff;
  
  const char * cbuf = buf.c_str();
  int size = buf.size();
  im->write_file(id, cbuf, size);
  
	assert(filemap[id]);
	filemap[id]->cached_cids.insert(cid);
	filemap[id]->working_cid = cid;
	notify(id, cid);
  return extent_protocol::OK;
}

int extent_server_cache::get(extent_protocol::extentid_t id, std::string cid, std::string &buf)
{
  tprintf("extent_server_cache: get %lld from %s\n", id, cid.c_str());

  id &= 0x7fffffff;

  int size = 0;
  char *cbuf = NULL;

	require(id, cid);
  im->read_file(id, &cbuf, &size);
  if (size == 0)
    buf = "";
  else {
    buf.assign(cbuf, size);
    free(cbuf);
  }

	assert(filemap[id]);
	filemap[id]->cached_cids.insert(cid);
	if(!filemap[id]->working_cid.empty())
		notify(id, cid);
  return extent_protocol::OK;
}

int extent_server_cache::getattr(extent_protocol::extentid_t id, std::string cid, extent_protocol::attr &a)
{
  tprintf("extent_server_cache: getattr %lld from %s\n", id, cid.c_str());

  id &= 0x7fffffff;
  extent_protocol::attr attr;
  memset(&attr, 0, sizeof(attr));

	require(id, cid);
  im->getattr(id, attr);
  a = attr;

	assert(filemap[id]);
	filemap[id]->cached_cids.insert(cid);
	if(!filemap[id]->working_cid.empty())
		notify(id, cid);
  return extent_protocol::OK;
}

int extent_server_cache::remove(extent_protocol::extentid_t id, std::string cid, int &)
{
  tprintf("extent_server_cache: remove %lld from %s\n", id, cid.c_str());
  id &= 0x7fffffff;

  im->remove_file(id);

	notify(id, cid);
	delete filemap[id];
	filemap.erase(filemap.find(id));
  return extent_protocol::OK;	
}

void extent_server_cache::notify(extent_protocol::extentid_t id, std::string cid)
{
	int r;
  tprintf("extent_server_cache: notify %lld\n", id);

	if(!filemap[id])
		return;
	if(filemap[id]->cached_cids.empty())
		return;

	for(std::set<std::string>::iterator it = filemap[id]->cached_cids.begin(); it != filemap[id]->cached_cids.end(); it ++)
		if(cid != *it){
			tprintf("\tnotify_accepter: %s\n", it->c_str());
			if(*it == filemap[id]->working_cid)
				filemap[id]->working_cid.clear();
			handle(*it).safebind()->call(extent_protocol::pull, id, r);
		}
}

void extent_server_cache::require(extent_protocol::extentid_t id, std::string cid)
{
	int r;
  tprintf("extent_server_cache: require %lld\n", id);

	if(!filemap[id])
		return;
	if(filemap[id]->working_cid.empty())
		return;

	tprintf("\trequire_accepter: %s\n", filemap[id]->working_cid.c_str());
	handle(filemap[id]->working_cid).safebind()->call(extent_protocol::push, id, r);
}
