#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>

#include <map>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"
#include <vector>
using namespace std;

struct lockinfo{
  bool isHold;
  string ID;
  vector<string> waitList;
  
  lockinfo(){
    isHold = false;
    ID = "";
  }
  void setId(string id){
    ID = id;
    isHold = true;
  }
  void reset(){
    ID = "";
    isHold = false;
  }
};

class lock_server_cache {
 private:
  pthread_mutex_t m_;
  pthread_cond_t c_;
  int nacquire;
  map<lock_protocol::lockid_t, lockinfo*> lockid_map;
 public:
  lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);
  void call_retry(lock_protocol::lockid_t, std::string id);
  void call_revoke(lock_protocol::lockid_t, std::string id);
};

#endif
