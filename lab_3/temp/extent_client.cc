// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

// The calls assume that the caller holds a lock on the extent

extent_client::extent_client(std::string dst)
{
  sockaddr_in dstsock;
  make_sockaddr(dst.c_str(), &dstsock);
  cl = new rpcc(dstsock);
  if (cl->bind() != 0) {
    printf("extent_client: bind failed\n");
  }
}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  ret = cl->call(extent_protocol::get, eid, buf);
  return ret;
}

extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid, 
		       extent_protocol::attr &attr)
{
  extent_protocol::status ret = extent_protocol::OK;
  ret = cl->call(extent_protocol::getattr, eid, attr);
  return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t p_eid, extent_protocol::extentid_t eid, std::string buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  int r;
  ret = cl->call(extent_protocol::put, p_eid, eid, buf, r);
  return ret;
}
extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
  extent_protocol::status ret = extent_protocol::OK;
  int r;
  ret = cl->call(extent_protocol::remove, eid, r);
  return ret;
}

extent_protocol::status
extent_client::write(extent_protocol::extentid_t eid, std::string buf, long long unsigned int off, size_t size)
{
  extent_protocol::status ret = extent_protocol::OK;
  int r;
  ret = cl->call(extent_protocol::write, eid, buf, off, size, r);
  return ret;
}

extent_protocol::status
extent_client::read(extent_protocol::extentid_t eid, size_t size, long long unsigned int off, std::string& buf){
  extent_protocol::status ret = extent_protocol::OK;
  ret = cl->call(extent_protocol::read, eid, size, off, buf);
  return ret;
}

extent_protocol::status
extent_client::setattr(extent_protocol::extentid_t eid, extent_protocol::attr attr)
{
  extent_protocol::status ret = extent_protocol::OK;
  ret = cl->call(extent_protocol::getattr, eid, attr);
  return ret;
}

extent_protocol::status
extent_client::unlink(extent_protocol::extentid_t p_eid, std::string name){
  extent_protocol::status ret = extent_protocol::OK;
  int r;
  ret = cl->call(extent_protocol::unlink, p_eid, name, r);
  return ret;
}
