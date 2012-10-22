// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

using namespace std;

extent_server::extent_server() {
  VERIFY(pthread_mutex_init(&m_, 0) == 0);
  contents t_content;
  
  extent_protocol::extentid_t root = 0x00000001;
  time_t t_ = time(NULL);
  dirs_map[root] = t_content;
  extent_protocol::attr a;
  a.ctime = t_;
  a.mtime = t_;
  a.atime = t_;
  a.size = 0;
  inods_map[root] = a;
}

int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &)
{
  // You fill this in for Lab 2.
  ScopedLock ml(&m_);
  extent_protocol::attr a;
  time_t t_ = time(NULL);
  inods::iterator it = inods_map.find(id);
  if(it != inods_map.end()){
    a = it->second;
  }
  a.ctime = t_;
  a.mtime = t_;
  a.atime = t_;
  a.size = 0;
  inods_map[id] = a;
  files_map[id] = "";

  extent_protocol::extentid_t root = 0x00000001;
  contents t_content = dirs_map[root];
  t_content[buf] = id;
  dirs_map[root] = t_content;

  it = inods_map.find(root);
  extent_protocol::attr a_ = it->second;
  a_.mtime = t_;
  a_.ctime = t_;
  inods_map[root] = a_;

  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
  ScopedLock ml(&m_);
  // You fill this in for Lab 2.
  inods::iterator it = inods_map.find(id);
  if(it == inods_map.end()){
     return extent_protocol::NOENT;
  }else{
    extent_protocol::attr a = it->second;
    a.atime = time(NULL);
    inods_map[id] = a;

    if(id & 0x80000000){
      files::iterator f_it = files_map.find(id);
      if(f_it != files_map.end())
	buf = f_it->second;
      else
	return extent_protocol::IOERR;
    }else{
      contents t_content = dirs_map[id];
      contents::iterator c_it;
      for(c_it=t_content.begin(); c_it!=t_content.end(); c_it++){
	stringstream ss;
	ss << c_it->first << ":" << c_it->second << "\n";
	buf.append(ss.str());
      }
    }
  }
  return extent_protocol::OK;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
  // You fill this in for Lab 2.
  // You replace this with a real implementation. We send a phony response
  // for now because it's difficult to get FUSE to do anything (including
  // unmount) if getattr fails.
  ScopedLock ml(&m_);

  a.size = 0;
  a.atime = 0;
  a.mtime = 0;
  a.ctime = 0;

  inods::iterator it = inods_map.find(id);
  if(it != inods_map.end()){
    extent_protocol::attr attr = it->second;
    a.ctime = attr.ctime;    
    a.atime = attr.atime;
    a.mtime = attr.mtime;
    a.size = attr.size;
  }else
    return extent_protocol::NOENT;

  return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
  // You fill this in for Lab 2.
  ScopedLock ml(&m_);

  inods::iterator it = inods_map.find(id);
  if(it == inods_map.end())
    return extent_protocol::NOENT;
  else
    inods_map.erase(it);
  
  return extent_protocol::OK;
}

int extent_server::write(extent_protocol::extentid_t id, std::string buff, long long unsigned int  off, size_t size, int &)
{
  ScopedLock ml(&m_);

  inods::iterator it = inods_map.find(id);
  if(it != inods_map.end()){
    files::iterator f_it = files_map.find(id);
    if(f_it != files_map.end()){
      string temp = "";
      string t_content = f_it->second;
      if(off == 0)
	temp = buff.substr(0, size);
      else if(off >= t_content.length())
	temp = t_content.append(off-t_content.length(), '\0').append(buff.c_str(), size);
      else
	temp = t_content.replace(off, size, buff.substr(0, size));
      
      files_map[id] = temp;
      inods::iterator i_it = inods_map.find(id);
      extent_protocol::attr a = i_it->second;
      a.mtime = time(NULL);
      a.size = temp.length();
      inods_map[id] = a;
      return extent_protocol::OK;
    }else
      return extent_protocol::IOERR;
  }else
    return extent_protocol::NOENT;
}

int extent_server::read(extent_protocol::extentid_t id, long long unsigned int off, size_t size, std::string& buf)
{
  ScopedLock ml(&m_);
  
  inods::iterator it = inods_map.find(id);
  if(it != inods_map.end()){
    files::iterator f_it = files_map.find(id);
    if(f_it != files_map.end()){
      string t_content = f_it->second;

      if(off < t_content.length()){
	if(off + size > t_content.length())
	  buf = t_content.substr(off);
	else
	  buf = t_content.substr(off, size);
      }else
	buf = "";

      inods::iterator i_it = inods_map.find(id);
      extent_protocol::attr a = i_it->second;
      a.atime = time(NULL);
      inods_map[id] = a;
      return extent_protocol::OK;

    }else
      return extent_protocol::IOERR;
  }else
    return extent_protocol::NOENT;
}

int extent_server::setattr(extent_protocol::extentid_t id, extent_protocol::attr &a){
  ScopedLock ml(&m_);
  a.ctime = time(NULL);
  inods_map[id] = a;
  return extent_protocol::OK;
}

