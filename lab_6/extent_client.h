// extent client interface.

#ifndef extent_client_h
#define extent_client_h

#include <string>
#include <vector>
#include "extent_protocol.h"
#include "lock_protocol.h"
#include "lock_client_cache.h"
#include "rpc.h"

#include <iostream>
#include <fstream>
#include "tprintf.h"

using namespace std;

typedef std::map<std::string, extent_protocol::extentid_t> contents;

struct file_info{
  string buf;
  extent_protocol::extentid_t p_id;
  file_info(string buf_, extent_protocol::extentid_t p_id_)
  :buf(buf_), p_id(p_id_){}
  file_info(string buf_)
  :buf(buf_), p_id(0){}
  file_info()
  :buf(""), p_id(0){}
};

struct dir_info{
  contents content;
  extent_protocol::extentid_t p_id;
  dir_info(contents content_, extent_protocol::extentid_t p_id_)
  :content(content_), p_id(p_id_){}
  dir_info(contents content_)
  :content(content_), p_id(0){}
  dir_info()
  :p_id(0){}
};

struct inod_info{
  extent_protocol::attr attr;
  bool dirty;
  inod_info(extent_protocol::attr attr_)
  :attr(attr_), dirty(false){}
  inod_info()
  :dirty(false){}
};

typedef std::map<extent_protocol::extentid_t, dir_info> dirs;
typedef std::map<extent_protocol::extentid_t, file_info> files;
typedef std::map<extent_protocol::extentid_t, inod_info> inods;

class extent_client :public lock_release_user{
 private:
  rpcc *cl;
  inods inods_map;
  dirs dirs_map;
  files files_map;
  pthread_mutex_t m_;
  vector<extent_protocol::extentid_t> remove_list;

 public:
  void dorelease(lock_protocol::lockid_t lid){
    tprintf("[dorelease] flush start\n");
    flush(lid);
    tprintf("[dorelease] flush end\n");
  }
  extent_client(std::string dst);

  extent_protocol::status get(extent_protocol::extentid_t eid, 
			      std::string &buf);
  extent_protocol::status getattr(extent_protocol::extentid_t eid, 
				  extent_protocol::attr &a);
  extent_protocol::status put(extent_protocol::extentid_t p_eid, extent_protocol::extentid_t eid, std::string buf);
  extent_protocol::status remove(extent_protocol::extentid_t eid);
  extent_protocol::status write(extent_protocol::extentid_t eid, std::string buf, long long unsigned int, size_t);
  extent_protocol::status read(extent_protocol::extentid_t eid, size_t, long long unsigned int, std::string&);
  extent_protocol::status setattr(extent_protocol::extentid_t eid, extent_protocol::attr);
  extent_protocol::status unlink(extent_protocol::extentid_t p_eid, std::string);
  extent_protocol::status checkDupInum(extent_protocol::extentid_t eid);
  extent_protocol::status flush(extent_protocol::extentid_t);
  std::string convertToString(contents&);
  void convertToContents(contents&, string);
};

#endif 

