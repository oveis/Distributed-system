// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include "lock_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  ec = new extent_client(extent_dst);
  lc = new lock_client(lock_dst);
}

yfs_client::inum
yfs_client::n2i(std::string n)
{
  std::istringstream ist(n);
  unsigned long long finum;
  ist >> finum;
  return finum;
}

std::string
yfs_client::filename(inum inum)
{
  std::ostringstream ost;
  ost << inum;
  return ost.str();
}

bool
yfs_client::isfile(inum inum)
{
  if(inum & 0x80000000)
    return true;
  return false;
}

bool
yfs_client::isdir(inum inum)
{
  return ! isfile(inum);
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
  extent_protocol::extentid_t eid = inum;
  int r = OK;
  // You modify this function for Lab 3
  // - hold and release the file lock


    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(eid, a) != extent_protocol::OK) {
      r = IOERR;
      goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

  release:

  return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
  extent_protocol::extentid_t eid = inum;
  int r = OK;
  // You modify this function for Lab 3
  // - hold and release the directory lock

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(eid, a) != extent_protocol::OK) {
      r = IOERR;
      goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

  release:

  return r;
}

int
yfs_client::get(inum ino, std::string &buf){
  extent_protocol::extentid_t eid = ino;
  int r = OK;

    if(ec->get(eid, buf) != extent_protocol::OK){
      r = IOERR;
      goto release;
    }
  release:

  return r;
}

int
yfs_client::put(inum p_ino, inum ino, std::string buf){
  extent_protocol::extentid_t eid = ino;
  extent_protocol::extentid_t p_eid = p_ino;
  int r = OK;
  if(lc->acquire(ino) == lock_protocol::OK){
    if(ec->put(p_eid, eid, buf) != extent_protocol::OK){
      r = IOERR;
      goto release;
    }
  release:
    lc->release(ino);
  }
  return r;
}

int
yfs_client::isExist(inum parent, std::string name, inum& inum){
  extent_protocol::status st;
  vector<struct dirent> vec;

    st = getDirEnts(parent, vec);
    bool isExist = false;

    if(st == OK){
      for(unsigned int i=0; i<vec.size(); i++){
	if(vec[i].name.compare(name) == 0){
	  inum = vec[i].inum;
	  isExist = true;
	  break;
	}
      }

      if(isExist)
	return EXIST;
      else
	return NOENT;

    }else{
      return st;
    }

}

int
yfs_client::getDirEnts(inum ino, vector<yfs_client::dirent>& vec){
  extent_protocol::status st;
  std::string buf;
  extent_protocol::extentid_t eid = ino;
    st = ec->get(eid, buf);

    stringstream ss(buf);
    string temp;

    while(ss >> temp){
      dirent ent;
      int cur = temp.find(":");
      ent.name = temp.substr(0, cur);
      ent.inum = n2i(temp.substr(cur+1));
      vec.push_back(ent);
    }

  return returnProtocol(st);
}

int
yfs_client::write(inum ino, std::string buf, off_t off, size_t size){
  extent_protocol::status st;
  if(lc->acquire(ino) == lock_protocol::OK){
    st = ec->write(ino, buf, off, size);
    lc->release(ino);
  }
  return returnProtocol(st);
}

int 
yfs_client::read(inum ino, size_t size, off_t off, string& buf){  
  extent_protocol::status st;
  if(lc->acquire(ino) == lock_protocol::OK){
    st = ec->read(ino, size, off, buf);
    lc->release(ino);
  }
  return returnProtocol(st);
}

int
yfs_client::setattr(inum ino, struct stat st){
  extent_protocol::attr a_;
  extent_protocol::status sta;
  if(lc->acquire(ino) == lock_protocol::OK){
    a_.atime = st.st_atime;
    a_.mtime = st.st_mtime;
    a_.ctime = st.st_ctime;
    a_.size = st.st_size;
    sta = ec->setattr(ino, a_);
    lc->release(ino);
  }
  return returnProtocol(sta);
}

int 
yfs_client::unlink(inum parent_inum, std::string name){
  extent_protocol::status st;
  if(lc->acquire(parent_inum) == lock_protocol::OK){
    st = ec->unlink(parent_inum, name);
    lc->release(parent_inum);
  }
  return returnProtocol(st);
}

int
yfs_client::returnProtocol(extent_protocol::status st){
  switch(st){
  case extent_protocol::OK:
    return OK;
  case extent_protocol::RPCERR:
    return RPCERR;
  case extent_protocol::NOENT:
    return NOENT;
  case extent_protocol::IOERR:
    return IOERR;
  case extent_protocol::EXIST:
    return EXIST;
  case extent_protocol::DUPID:
    return DUPID;
  default:
    return RPCERR;
  }
}

