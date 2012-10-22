// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
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
  int r = OK;

  printf("getfile %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
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
  int r = OK;

  printf("getdir %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
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
yfs_client::put(inum ino, std::string buf){
  extent_protocol::extentid_t eid = ino;
  int r = OK;
  if(ec->put(eid, buf) != extent_protocol::OK){
    r = IOERR;
    goto release;
  }
 release:
  return r;
}

int
yfs_client::create(inum ino, const char *name)
{
  int r = OK;
  extent_protocol::extentid_t eid = ino;
  r = ec->put(eid, name);
  return r;
}

bool
yfs_client::isExist(inum parent, const char *name, inum &inum){
  vector<struct dirent> vec;
  getAllFile(parent, vec);
  bool isExist = false;
  for(int i=0; i<vec.size(); i++){
    if(vec[i].name.compare(name) == 0){
      inum = vec[i].inum;
      isExist = true;
    }
  }
  return isExist;
}

int
yfs_client::getAllFile(inum ino, vector<yfs_client::dirent>& vec){
  int r = OK;
  std::string buf;
  extent_protocol::extentid_t eid = ino;
  ec->get(eid, buf);

  stringstream ss(buf);
  string temp;

  while(ss >> temp){
    dirent t_dirent;
    int cur = temp.find(":");
    t_dirent.name = temp.substr(0, cur);
    t_dirent.inum = n2i(temp.substr(cur+1));
    vec.push_back(t_dirent);
  }

  return r;
}

int
yfs_client::write(inum ino, const std::string buf, off_t off, size_t size){
  extent_protocol::status st = ec->write(ino, buf, off, size);
  if(st == extent_protocol::OK)
    return OK;
  else if(st == extent_protocol::NOENT)
    return NOENT;
  else
    return IOERR;
}

int 
yfs_client::read(inum ino, off_t off, size_t size, string& buf){
  extent_protocol::status st = ec->read(ino, off, size, buf);
  if(st == extent_protocol::OK)
    return OK;
  else if(st == extent_protocol::NOENT)
    return NOENT;
  else
    return IOERR;
}

int
yfs_client::setattr(inum ino, extent_protocol::attr& attr){
  extent_protocol::status st = ec->setattr(ino, attr);
  if(st == extent_protocol::OK)
    return OK;
  else if(st == extent_protocol::NOENT)
    return NOENT;
  else
    return IOERR;
}
