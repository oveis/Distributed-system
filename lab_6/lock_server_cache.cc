// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"

lock_server_cache::lock_server_cache()
{
  pthread_mutex_init(&m_, NULL);
  pthread_cond_init(&c_, NULL);
}


int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, 
                               int &)
{
  lock_protocol::status ret; // = lock_protocol::OK;
  lockinfo *li;
  {
    ScopedLock ml(&m_);
    // New one
    if(lockid_map.find(lid) == lockid_map.end()){
      tprintf("acquire : new\n");
      ret = lock_protocol::OK;
      li = new lockinfo();
      li->setId(id);
      lockid_map[lid] = li;
      // If there are waiting thread, 
      if(li->waitList.size() > 0){
	goto RPC_CALL_REVOKE;
      }
    }
    // Not New one
    else{
      li = lockid_map[lid];
      // Free
      if(!li->isHold){
	tprintf("acquire : free\n");
	ret = lock_protocol::OK;
	li->setId(id);
	// If there are waiting thread, 
	if(li->waitList.size() > 0){
	  goto RPC_CALL_REVOKE;
	}
      }
      // Someone hold it
      else{
	tprintf("acquire : holding\n");
	ret = lock_protocol::RETRY;
	// Check already sent the 'revoke' message
	bool exist = false;
	vector<string> waitList = li->waitList;
	for(int i=0; i<waitList.size(); i++){
	  if(waitList[i] == id){
	    exist = true;
	    break;
	  }
	}
	/*
	if(id == li->ID)
	  return ret;
	*/

	if(!exist){  // If not sent 'revoke'
	  li->waitList.push_back(id);
	  goto RPC_CALL_REVOKE;
	}
      }
    }
  }
  return ret;

 RPC_CALL_REVOKE:
  call_revoke(lid, li->ID);
  return ret;
}

void
lock_server_cache::call_revoke(lock_protocol::lockid_t lid, std::string id){
  cout << "call revoke :  (" << id << ")" << endl;
  handle h(id);
  rpcc *cl = h.safebind();
  if(cl){
    int r;
    cl->call(rlock_protocol::revoke, lid, r);
  }else{
    // handle failure
  }
}

int 
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, 
         int &r)
{
  lockinfo *li;
  lock_protocol::status ret;
  {
    ScopedLock ml(&m_);
    
    if(lockid_map.find(lid) == lockid_map.end()){
      tprintf("release : error\n");
      ret = lock_protocol::OK;
    }else{
      ret = lock_protocol::OK;
      li = lockid_map[lid];
      lockinfo *li = lockid_map[lid];
      li->reset();
      tprintf("release : complete\n");

      // retry to the next waiting client
      vector<string> waitList = li->waitList;
      if(waitList.size() <= 0){
	tprintf("release : not wait list\n");
	//lockid_map.erase(lockid_map.find(lid));
      }else{
	tprintf("release : wait list (%d)\n", waitList.size());
	string next_id = waitList.front();
	waitList.erase(waitList.begin());
	li->waitList =  waitList;
	call_retry(lid, next_id);
      }
    }
  }
  tprintf("release : end");
  return ret;
}

void 
lock_server_cache::call_retry(lock_protocol::lockid_t lid, std::string id){
  handle h(id);
  int re;
  rpcc *cl = h.safebind();
  if(cl){
    cl->call(rlock_protocol::retry, lid, re);
  }else{
    cout << "release : ERROR" << endl;
  }
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  tprintf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}

