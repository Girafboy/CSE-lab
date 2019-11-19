// RPC stubs for clients to talk to extent_server

#include "extent_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

int extent_client_cache::last_port = 0;

extent_client_cache::extent_client_cache(std::string dst) : extent_client(dst)
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
  ret = cl->call(extent_protocol::create, type, id, eid);
  VERIFY(ret == extent_protocol::OK);
	assert(filemap[eid] == NULL);
	filemap[eid] = new fileinfo();
	filemap[eid]->attr.type = type;
	filemap[eid]->attr_dirty = false;
	filemap[eid]->buf_dirty = false;
  return ret;
}

extent_protocol::status
extent_client_cache::get(extent_protocol::extentid_t eid, std::string &buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  VERIFY(ret == extent_protocol::OK);
  
	if(filemap[eid] == NULL)
		filemap[eid] = new fileinfo();
		
  fileinfo *info = filemap[eid];
  if(info->buf_dirty){
  	ret = cl->call(extent_protocol::get, eid, id, buf);
		info->buf.assign(buf);
		info->buf_dirty = false;
	} else
		buf.assign(info->buf);

  return ret;
}

extent_protocol::status
extent_client_cache::getattr(extent_protocol::extentid_t eid, 
		       extent_protocol::attr &attr)
{
  int ret = extent_protocol::OK;
  
	if(filemap[eid] == NULL)
		filemap[eid] = new fileinfo();

  fileinfo *info = filemap[eid];
  if(info->attr_dirty){
    ret = cl->call(extent_protocol::getattr, eid, id, attr);
		info->attr = attr;
		info->attr_dirty = false;
	} else
		attr = info->attr;
  return ret;
}

extent_protocol::status
extent_client_cache::put(extent_protocol::extentid_t eid, std::string buf)
{
  int r;
  extent_protocol::status ret = extent_protocol::OK;
  ret = cl->call(extent_protocol::put, eid, id, buf, r);
  VERIFY(ret == extent_protocol::OK);
  
	if(filemap[eid]){
		filemap[eid]->attr.atime = time(NULL);
		filemap[eid]->attr.mtime = time(NULL);
		filemap[eid]->attr.ctime = time(NULL);
		filemap[eid]->attr.size = buf.size();
		filemap[eid]->buf.assign(buf);
		filemap[eid]->attr_dirty = false;
		filemap[eid]->buf_dirty = false;
	}
  return ret;
}

extent_protocol::status
extent_client_cache::remove(extent_protocol::extentid_t eid)
{
  int r;
  extent_protocol::status ret = extent_protocol::OK;
  ret = cl->call(extent_protocol::remove, eid, id, r);
  VERIFY(ret == extent_protocol::OK);
	filemap.erase(filemap.find(eid));
  return ret;
}

extent_protocol::status extent_client_cache::pull_handler(extent_protocol::extentid_t eid, int &)
{
	filemap[eid]->attr_dirty = true;
	filemap[eid]->buf_dirty = true;
  return extent_protocol::OK;
}

extent_protocol::status extent_client_cache::push_handler(extent_protocol::extentid_t eid, int &)
{
  return extent_protocol::OK;
}
