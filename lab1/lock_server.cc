// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

pthread_mutex_t m_;
pthread_cond_t c_;
std::map<lock_protocol::lockid_t, bool> lockid_map;

lock_server::lock_server():
  nacquire (0)
{
  pthread_mutex_init(&m_, NULL);
  pthread_cond_init(&c_, NULL);
}

lock_protocol::status
lock_server::grandLock(int clt, lock_protocol::lockid_t lid, int &r)
{
  ScopedLock ml(&m_);
  printf("clt: %d, lock_id: %d\n, lockid_map: %d", clt, lid, lockid_map[lid]);
  while(lockid_map[lid])
    pthread_cond_wait(&c_, &m_);

  lockid_map[lid] = true;

  lock_protocol::status ret = lock_protocol::OK;
  r = nacquire;
  return ret;
}

lock_protocol::status
lock_server::releaseLock(int clt, lock_protocol::lockid_t lid, int &r)
{
  ScopedLock ml(&m_);
  lockid_map[lid] = false;
  pthread_cond_broadcast(&c_);
  lock_protocol::status ret = lock_protocol::OK;
  r = nacquire;
  return ret;
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}




