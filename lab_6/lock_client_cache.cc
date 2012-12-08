// RPC stubs for clients to talk to lock_server, and cache the locks
<<<<<<< HEAD
// see lock_client.cache.h for protocol details.
=======
// see lock_client_cache.h for protocol details.
>>>>>>> lab5

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"

<<<<<<< HEAD

=======
>>>>>>> lab5
lock_client_cache::lock_client_cache(std::string xdst, 
				     class lock_release_user *_lu)
  : lock_client(xdst), lu(_lu)
{
  rpcs *rlsrpc = new rpcs(0);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);

  const char *hname;
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlsrpc->port();
  id = host.str();
<<<<<<< HEAD
=======
  pthread_mutex_init(&m_, NULL);
>>>>>>> lab5
}

lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
<<<<<<< HEAD
  int ret = lock_protocol::OK;
  return lock_protocol::OK;
=======
  tprintf("[acquire lock] lid:%d\n", lid);
  lock_protocol::status ret = lock_protocol::OK;
  cl_lockinfo *cli = NULL;
 
  {
    ScopedLock ml(&m_);
    if(cl_lockid_map.find(lid) == cl_lockid_map.end()){
      cli = new cl_lockinfo;
      cl_lockid_map[lid] = cli;
    }else{
      cli = cl_lockid_map[lid];
    }
  }

  {
    ScopedLock lid_ml(&cli->lid_m_);

    while(true){
      if(cli->status == NONE){
	cli->status = ACQUIRING;
	break;
      }else if(cli->status == FREE){
	tprintf("acquire : free -> locked (%s)\n", id.c_str());

	cli->status = LOCKED;
	cli->thread_id = pthread_self();
	return lock_protocol::OK;
      }else{   // LOCKED, ACQUIRING, RELEASING
	pthread_cond_wait(&cli->lid_c_, &cli->lid_m_);
      }
    }
  }

  while(true){
    int r;
    ret = cl->call(lock_protocol::acquire, lid, id, r);
        
    ScopedLock lid_ml(&cli->lid_m_);
    // Acquired
    if(ret == lock_protocol::OK){
      cli->status = LOCKED;
      cli->isRetry = false;
      cli->thread_id = pthread_self();
      tprintf("acquire : acquire done (%s)\n", id.c_str());
      break;
    }
    // Not Aquired
    else if(ret == lock_protocol::RETRY){
      /*
      while(!cli->isRetry)
	pthread_cond_wait(&cli->retry_c_, &cli->lid_m_);
      cli->isRetry = false;
      */
      if(cli->isRetry){
	cli->isRetry = false;
	continue;
      }else{
	//while(!cli->isRetry)
	  pthread_cond_wait(&cli->retry_c_, &cli->lid_m_);
      }
    }
  }
  return ret;
>>>>>>> lab5
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
<<<<<<< HEAD
  return lock_protocol::OK;

=======
  cout << "[release lock] lid: " << lid << endl;
  // If there are no revoke RPC from server,
  // wake up other thread for waiting this lid.
  lock_protocol::status ret = lock_protocol::OK;
  cl_lockinfo *cli = NULL;

  {
    ScopedLock ml(&m_);
  
    if(cl_lockid_map.find(lid) != cl_lockid_map.end()){
      cli = cl_lockid_map[lid];
    }else{
      // error
      cout << "? lid : " << lid << endl;
      cout << "release : ERROR " << endl;
      return lock_protocol::IOERR;
    }
  }
  {
    ScopedLock lid_ml(&cli->lid_m_);
    
    if(cli->isRevoke){
      cli->status = RELEASING;
      cli->thread_id = 0;
      goto RPC_CALL_RELEASE; 
    }else{
      tprintf("release : not revoked : free (%s)\n", id.c_str());
      cli->status = FREE;
      cli->thread_id = 0;
      pthread_cond_broadcast(&cli->lid_c_);
      return lock_protocol::OK;
    }
  }
   
 RPC_CALL_RELEASE:
  tprintf("release : rpc call release (%s)\n", id.c_str());
  int r;
  lu->dorelease(lid); // flush

  tprintf("<<<<< release dorelease [%d] >>>>>\n", lid);

  ret = cl->call(lock_protocol::release, lid, id, r);
  {
    VERIFY (ret == lock_protocol::OK);
    ScopedLock lid_ml(&cli->lid_m_);
    tprintf("release : release complete (%s)\n", id.c_str());
    cli->status = NONE;
    cli->isRevoke = false;
    pthread_cond_broadcast(&cli->lid_c_);
  }
  return ret;
>>>>>>> lab5
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, 
                                  int &)
{
<<<<<<< HEAD
  int ret = rlock_protocol::OK;
=======
  int ret = rlock_protocol::OK; 
  cl_lockinfo *cli = NULL;

  {
    ScopedLock ml(&m_);
    cli = cl_lockid_map[lid];
  }
  {
    ScopedLock lid_ml(&cli->lid_m_);

    cli->isRevoke = true;
    if(cli->status == FREE){
      cli->status = RELEASING;
      cli->thread_id = 0;
      goto RPC_CALL_RELEASE;
    }else{
      cli->status = RELEASING;
      return ret;
    }
  }

 RPC_CALL_RELEASE:
  lock_protocol::status temp_ret;
  int r;
  lu->dorelease(lid); // flush
  tprintf("<<<<< revoke dorelease [%d] >>>>>\n", lid);
  temp_ret = cl->call(lock_protocol::release, lid, id, r);
  {
    ScopedLock lid_ml(&cli->lid_m_);
    if(temp_ret == lock_protocol::OK){
      cli->status = NONE;
      cli->isRevoke = false;
      pthread_cond_broadcast(&cli->lid_c_);
    }
  } 
>>>>>>> lab5
  return ret;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, 
                                 int &)
{
  int ret = rlock_protocol::OK;
<<<<<<< HEAD
=======
  cl_lockinfo *cli = NULL;

  if(cl_lockid_map.find(lid) != cl_lockid_map.end()){
    cli = cl_lockid_map[lid];
  }else{
    // error
    cout << "release : ERROR " << endl;
    return lock_protocol::IOERR;
  }

  {
    ScopedLock lid_ml(&cli->lid_m_);
    if(cli->status == ACQUIRING)
      cli->isRetry = true;
    pthread_cond_signal(&cli->retry_c_);
  }
  return ret;
}

lock_protocol::status
lock_client_cache::get_lock_status(lock_protocol::lockid_t lid){
  lock_protocol::status ret;
  if(cl_lockid_map.find(lid) != cl_lockid_map.end()){
    cl_lockinfo *cli = cl_lockid_map[lid];
    ret = cli->status;
  }else{
    ret = NONE;
  }
>>>>>>> lab5
  return ret;
}



