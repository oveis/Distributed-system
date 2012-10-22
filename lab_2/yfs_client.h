#ifndef yfs_client_h
#define yfs_client_h

#include <string>
//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>
#include <string>
#include <time.h>

using namespace std;

class yfs_client {
  extent_client *ec;
 public:

  typedef unsigned long long inum;
  enum xxstatus { OK, RPCERR, NOENT, IOERR, EXIST };
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
  int put(inum ino, std::string buf);
  int create(inum ino, const char *name);
  bool isExist(inum parent, const char *name, inum &);
  int getAllFile(inum ino, vector<dirent> &);

  int write(inum, const std::string, off_t, size_t);
  int read(inum, off_t, size_t, std::string&);
  int setattr(inum, extent_protocol::attr&);
};

#endif 
