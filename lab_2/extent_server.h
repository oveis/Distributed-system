// this is the extent server

#ifndef extent_server_h
#define extent_server_h

#include <string>
#include <map>
#include "extent_protocol.h"

typedef std::map<std::string, extent_protocol::extentid_t> contents;
typedef std::map<extent_protocol::extentid_t, extent_protocol::attr> inods;
typedef std::map<extent_protocol::extentid_t, contents> dirs;
typedef std::map<extent_protocol::extentid_t, std::string> files;

class extent_server {
 private:
  inods inods_map;
  dirs dirs_map;
  files files_map;
  pthread_mutex_t m_;
 public:
  extent_server();

  int put(extent_protocol::extentid_t id, std::string, int &);
  int get(extent_protocol::extentid_t id, std::string &);
  int getattr(extent_protocol::extentid_t id, extent_protocol::attr &);
  int remove(extent_protocol::extentid_t id, int &);
  int write(extent_protocol::extentid_t, const std::string, long long unsigned int, size_t, int&);
  int read(extent_protocol::extentid_t, long long unsigned int, size_t, std::string&);
  int setattr(extent_protocol::extentid_t, extent_protocol::attr &);
};

#endif 







