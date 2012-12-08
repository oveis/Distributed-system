#ifndef extent_client_cache_h
#define extent_client_cache_h

#include <string>

using namespace std;

typedef std::map<extent_protocol::extentid_t, Content> contents;
typedef std::map<extent_protocol::extentid_t, extent_protocol::attr> inods;
typedef std::map>extent_protocol::extentid_t, bool> status;
typedef std::map>extent_protocol::extentid_t, bool> status;

class extent_client_cache{
 private:
  pthread_mutex_t m_;

 public:
  status dirty_map;   // true : dirted
  inods inods_map;
  dirs dirs_map;
  files files_map;

};

#endif

