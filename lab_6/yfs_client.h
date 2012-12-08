#ifndef yfs_client_h
#define yfs_client_h

#include <string>
//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>
#include <string>
#include <time.h>
#include "lock_protocol.h"
#include "lock_client.h"
#include "lock_client_cache.h"
using namespace std;

class yfs_client {
  extent_client *ec;
  //lock_client *lc;
  lock_client_cache *lc;

 public:
  
  typedef unsigned long long inum;
  enum xxstatus { OK, RPCERR, NOENT, IOERR, EXIST, DUPINUM };
  typedef int status;

  struct fileinfo {
    unsigned long long size;
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirinfo {
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirent {
    std::string name;
    yfs_client::inum inum;
  };

 private:
  static std::string filename(inum);
  static inum n2i(std::string);

 public:

  yfs_client(std::string, std::string);

  bool isfile(inum);
  bool isdir(inum);

  int getfile(inum inum, fileinfo &);
  int getdir(inum inum, dirinfo &);

  int get(inum ino, std::string &buf);
  int put(inum p_ino, inum ino, std::string buf);
  int create(inum, const char *name, inum);
  int isExist(inum parent, std::string name, inum &inum);
  int getDirEnts(inum ino, vector<dirent> &);

  int write(inum, std::string, off_t, size_t);
  int read(inum, size_t, off_t, std::string&);
  int setattr(inum, struct stat);
  int unlink(inum, std::string);
  int returnProtocol(extent_protocol::status);  
};

#endif 
