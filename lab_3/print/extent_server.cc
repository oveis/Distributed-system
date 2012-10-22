// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
using namespace std;

extent_server::extent_server() {

  VERIFY(pthread_mutex_init(&m_, 0) == 0);
  contents t_content;
  root_id = 0x00000001;

  dirs_map[root_id] = t_content;
  extent_protocol::attr root_attr;
  root_attr.ctime = root_attr.mtime = root_attr.atime = (int)time(NULL);
  root_attr.size = 0;
  inods_map[root_id] = root_attr;
}

int extent_server::put(extent_protocol::extentid_t p_id, extent_protocol::extentid_t id, std::string buf, int &)
{
  // You fill this in for Lab 2.
  ScopedLock ml(&m_);
  
  // new file/dir
  extent_protocol::attr new_attr;
  new_attr.atime = new_attr.mtime = new_attr.ctime = (int)time(NULL);
  new_attr.size = 0;
  inods_map[id] = new_attr;

  if(id & 0x80000000)    // File
    files_map[id] = "";
  else{                  // Directory
    contents c_;
    dirs_map[id] = c_;
  }

  // parent
  contents content = dirs_map[p_id];
  content[buf] = id;
  dirs_map[p_id] = content;

  // parent attr
  extent_protocol::attr parent_attr = inods_map[p_id];
  parent_attr.mtime = parent_attr.ctime = (int)time(NULL);
  parent_attr.size = content.size();
  inods_map[p_id] = parent_attr;

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
    a.atime = (int)time(NULL);
    inods_map[id] = a;

    // File
    if(id & 0x80000000){
      files::iterator f_it = files_map.find(id);
      if(f_it != files_map.end())
	buf = f_it->second;
      else
	return extent_protocol::IOERR;
    }
    // Directory
    else{
      contents content = dirs_map[id];
      contents::iterator c_it;
      for(c_it=content.begin(); c_it!=content.end(); c_it++){
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

    return extent_protocol::OK;
  }else
    return extent_protocol::NOENT;
}

int extent_server::setattr(extent_protocol::extentid_t id, extent_protocol::attr attr, int &){
  ScopedLock ml(&m_);
  inods_map[id] = attr;
  return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
  // You fill this in for Lab 2.
  ScopedLock ml(&m_);

  inods::iterator it = inods_map.find(id);
  if(it == inods_map.end())
    return extent_protocol::NOENT;
  else{
    inods_map.erase(it);
  }
  return extent_protocol::OK;
}

//
// Write to file
//

int extent_server::write(extent_protocol::extentid_t id, std::string buff, long long unsigned int  off, size_t size, int &)
{
  ScopedLock ml(&m_);

  inods::iterator it = inods_map.find(id);
  if(it != inods_map.end()){
    files::iterator f_it = files_map.find(id);
    if(f_it != files_map.end()){
      string content = f_it->second;
      int content_length = inods_map[id].size;
      string temp = "";
      if(off == 0 && size == 0){
	temp = "";
	goto release;
      }

      if(off >= content.length()){
	temp = content.append(off - content.length(), '\0').append(buff.substr(0, size));
      }else if(off + size > content.length()){
	temp = content.substr(0, off).append(buff.substr(0, size));
      }else{
	temp = content.replace(off, size, buff.substr(0, size));
      }

    release:
      
      files_map[id] = temp;
      extent_protocol::attr attr = it->second;
      attr.mtime = attr.ctime = (int)time(NULL);
      attr.size = temp.length();
      inods_map[id] = attr;
    }else
      return extent_protocol::IOERR;
  }else
    return extent_protocol::NOENT;

  return extent_protocol::OK;

}

//
// Read from file
//

int extent_server::read(extent_protocol::extentid_t id, size_t size, long long unsigned int off, std::string& buf)
{
  ScopedLock ml(&m_);

  inods::iterator it = inods_map.find(id);
  if(it != inods_map.end()){
    files::iterator f_it = files_map.find(id);
    if(f_it != files_map.end()){
      //string content = f_it->second;
      unsigned int file_size = inods_map[id].size;
      string content = f_it->second;

      // content.length -> size
      if(off >= file_size){
	buf = content.substr(0, file_size);
      }else if(file_size - off < size){
	buf = content.substr(off, file_size);
      }else{
	buf = content.substr(off, size);
      }
    
      //inods::iterator i_it = inods_map.find(id);
      extent_protocol::attr attr = it->second;
      attr.atime = (int)time(NULL);
      inods_map[id] = attr;
    }else
      return extent_protocol::IOERR;
  }else
    return extent_protocol::NOENT;

  return extent_protocol::OK;
}

//
// Remove the file named @name from directory @parent(p_id)
// Free the file's extent.
// If the file doesn't exist, indicate error ENOENT.
//
// Do *not* allow unlinking of a directory.
//
int extent_server::unlink(extent_protocol::extentid_t p_id, std::string name, int &){
  ScopedLock ml(&m_);

  // Search the parent directory's inodes
  inods::iterator p_inod_it = inods_map.find(p_id);
  if(p_inod_it == inods_map.end()){
    return extent_protocol::NOENT;
  }else{
    // search parent inode
    extent_protocol::attr parent_attr = p_inod_it->second;
    
    // Get Parent directory's content
    dirs::iterator p_dir_it = dirs_map.find(p_id);
    contents content;
    if(p_dir_it == dirs_map.end())
      return extent_protocol::NOENT;
    else
      content = p_dir_it->second;

    // Search file name in the parent directory's content
    contents::iterator c_it = content.find(name);
    if(c_it == content.end()){
      // Modify parent's attribute
      return extent_protocol::NOENT;
    }else{
      // free file's extent
      extent_protocol::extentid_t obj_id = c_it->second;

      if(obj_id & 0x80000000){   // File
	content.erase(name);
	dirs_map[p_id] = content;
	files_map.erase(obj_id);      
	inods_map.erase(obj_id);
      
	// Modify parent's attribute
	parent_attr.mtime = parent_attr.ctime = (int)time(NULL);
	inods_map[p_id] = parent_attr;
	return extent_protocol::OK;

      }else{                       // Directory
	dirs_map.erase(obj_id);
	inods_map.erase(obj_id);
	// Modify parent's attribute
	parent_attr.mtime = parent_attr.ctime = (int)time(NULL);
	inods_map[p_id] = parent_attr;
	return extent_protocol::OK;
      }
    }
  }
}

int extent_server::checkDupInum(extent_protocol::extentid_t eid, int &){
  inods::iterator it = inods_map.find(eid);
  if(it == inods_map.end())
    return extent_protocol::OK;
  else
    return extent_protocol::DUPINUM;
}
