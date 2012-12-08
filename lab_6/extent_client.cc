// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include "tprintf.h"

using namespace std;

// The calls assume that the caller holds a lock on the extent

extent_client::extent_client(std::string dst)
{
  sockaddr_in dstsock;
  make_sockaddr(dst.c_str(), &dstsock);
  cl = new rpcc(dstsock);
  if (cl->bind() != 0) {
    printf("extent_client: bind failed\n");
  }
  VERIFY(pthread_mutex_init(&m_, 0) == 0);

}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  
  // Check inods
  if(inods_map.find(eid) == inods_map.end()){
    extent_protocol::status ret_;
    extent_protocol::attr attr_;
    ret_ = cl->call(extent_protocol::getattr, eid, attr_);	
    if(ret_ == extent_protocol::OK){
      inod_info ii(attr_);
      inods_map[eid] = ii;
    }else{
      return ret_;      
    }
  }

  // Check File map / Directory map
  if(eid & 0x80000000){  // File
    if(files_map.find(eid) == files_map.end()){
      string buf_;
      extent_protocol::status ret_;
      ret_ = cl->call(extent_protocol::get, eid, buf_);
      if(ret_ == extent_protocol::OK){
	file_info fi(buf_);
	files_map[eid] = fi;
      }else
	return ret_;
    }
  }else{                 // Directory
    if(dirs_map.find(eid) == dirs_map.end()){
      string buf_;
      extent_protocol::status ret_;
      ret_ = cl->call(extent_protocol::get, eid, buf_);
      if(ret_ == extent_protocol::OK){
	contents content;
	convertToContents(content, buf_);
	dir_info di(content);
	dirs_map[eid] = di;
      }else
	return ret_;
    }
  }

  // Return Cached value
  if(eid & 0x80000000)  // File
    buf = files_map[eid].buf;
  else{                   // Directory
    buf = convertToString( dirs_map[eid].content);
  }

  // Attribute
  inod_info ii = inods_map[eid];
  ii.attr.atime = (int)time(NULL);
  ii.dirty = true;
  inods_map[eid] = ii; 

  return extent_protocol::OK;
}

extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid, 
		       extent_protocol::attr &attr)
{
  extent_protocol::status ret = extent_protocol::OK;

  if(inods_map.find(eid) == inods_map.end()){
    extent_protocol::attr a_;
    ret = cl->call(extent_protocol::getattr, eid, a_);
    if(ret == extent_protocol::OK){
      inod_info ii(a_);
      inods_map[eid] = ii;
    }else
      return ret;
  }
  attr = inods_map[eid].attr;
  return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t p_eid, extent_protocol::extentid_t eid, std::string buf)
{
  ScopedLock ml(&m_);

  extent_protocol::status ret = extent_protocol::OK;
  int r;

  // replace the cached copy
  extent_protocol::attr new_attr;
  new_attr.atime = new_attr.mtime = new_attr.ctime = (int)time(NULL);
  new_attr.size = 0;
  inod_info ii(new_attr);
  ii.dirty = true;
  inods_map[eid] = ii;

  if(eid & 0x80000000){   // File
    file_info fi("", p_eid);
    files_map[eid] = fi;
  }else{                   // Directory
    dir_info di;
    dirs_map[eid] = di;
  }

  // Parent :check existence in cache
  if(inods_map.find(p_eid) == inods_map.end()){
    extent_protocol::status r_;
    extent_protocol::attr a_;
    r_ = cl->call(extent_protocol::getattr, p_eid, a_);
    if(r_ == extent_protocol::OK){
      inod_info ii(a_);
      inods_map[p_eid] = ii;
    }else
      return ret;
  }

  if(dirs_map.find(p_eid) == dirs_map.end()){
    extent_protocol::status r_;
    string buf_;
    r_ = cl->call(extent_protocol::get, p_eid, buf_);
    if(r_ == extent_protocol::OK){
      contents content;
      convertToContents(content, buf_);
      dir_info di(content);
      dirs_map[p_eid] = di;
    }else
      return r_;
  }

  // Parent : modify
  contents content = dirs_map[p_eid].content;
  content[buf] = eid;
  dirs_map[p_eid].content = content;

  // Parent :: attr modify
  inod_info p_ii = inods_map[p_eid];
  p_ii.attr.mtime = p_ii.attr.ctime = (int)time(NULL);
  p_ii.attr.size = dirs_map[p_eid].content.size();
  p_ii.dirty = true;
  inods_map[p_eid] = p_ii;

  return extent_protocol::OK;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
  ScopedLock ml(&m_);

  tprintf("[remove] start\n");

  inods::iterator i_it = inods_map.find(eid);
  // If exist in the cache
  if(i_it != inods_map.end()){
    inods_map.erase(i_it);
    if(eid & 0x80000000){
      files_map.erase( files_map.find(eid) );
    }else{
      dirs_map.erase( dirs_map.find(eid) );
    }
    remove_list.push_back(eid);
  }
  // If not exist in the cache
  else{
    //If alread removed
    for(int i=0; i<remove_list.size(); i++)
      if(remove_list[i] == eid)
	return extent_protocol::NOENT;
    // Check in the server
    extent_protocol::status ret_;
    string buf_;
    ret_ = cl->call(extent_protocol::get, eid, buf_);
    if(ret_ == extent_protocol::OK)
      remove_list.push_back(eid);
    else
      return ret_;
  }

  tprintf("[remove] end\n");

  return extent_protocol::OK;
}

extent_protocol::status
extent_client::write(extent_protocol::extentid_t eid, std::string buf, long long unsigned int off, size_t size)
{
  ScopedLock ml(&m_);

  extent_protocol::status ret = extent_protocol::OK;
  int r;
  extent_protocol::attr attr;
  string content;

  // Get attribute from extent_server
  if(inods_map.find(eid) == inods_map.end()){
    ret = cl->call(extent_protocol::getattr, eid, attr);
    if(ret == extent_protocol::OK){
      inod_info ii(attr);
      inods_map[eid] = ii;
    }else
      return extent_protocol::NOENT;
  }

  // Get content from extent_server
  if(files_map.find(eid) == files_map.end()){
    ret = cl->call(extent_protocol::get, eid, content);
    if(ret == extent_protocol::OK){
      file_info fi(content);
      files_map[eid] = fi;
    }else
      return extent_protocol::NOENT;
  }

  content = files_map[eid].buf;
  int content_length = content.length();
  string temp = "";
  if(off == 0 && size == 0){
    temp = "";
    goto release;
  }

  if(off >= content.length()){
    temp = content.append(off - content.length(), '\0').append(buf.substr(0, size));
  }else if(off + size > content.length()){
    temp = content.substr(0, off).append(buf.substr(0, size));
  }else{
    temp = content.replace(off, size, buf.substr(0, size));
  }

 release:
  files_map[eid].buf = temp;
  attr = inods_map[eid].attr;
  attr.mtime = attr.ctime = (int)time(NULL);
  attr.size = temp.length();
  inods_map[eid].attr = attr;
  inods_map[eid].dirty = true;

  return extent_protocol::OK;
}

extent_protocol::status
extent_client::read(extent_protocol::extentid_t eid, size_t size, long long unsigned int off, std::string& buf){
  
  extent_protocol::status ret = extent_protocol::OK;
  extent_protocol::attr attr;
  string content;

  // Get attribute from extent_server
  if(inods_map.find(eid) == inods_map.end()){
    ret = cl->call(extent_protocol::getattr, eid, attr);
    if(ret == extent_protocol::OK){
      inod_info ii(attr);
      inods_map[eid] = ii;
    }else
      return extent_protocol::NOENT;
  }

  // Get content from extent_server
  if(files_map.find(eid) == files_map.end()){
    ret = cl->call(extent_protocol::get, eid, content);
    if(ret == extent_protocol::OK){
      file_info fi(content);
      files_map[eid] = fi;
    }else
      return extent_protocol::NOENT;
  }

  unsigned int file_size = files_map[eid].buf.length();
  content = files_map[eid].buf;

  // content.length -> size
  if(off >= file_size){
    buf = content.substr(0, file_size);
  }else if(file_size - off < size){
    buf = content.substr(off, file_size);
  }else{
    buf = content.substr(off, size);
  }
    
  inods_map[eid].attr.atime = (int)time(NULL);
  inods_map[eid].dirty = true;
  ret = extent_protocol::OK;

  return ret;
}

extent_protocol::status
extent_client::setattr(extent_protocol::extentid_t eid, extent_protocol::attr attr)
{
  ScopedLock ml(&m_);
  extent_protocol::status ret = extent_protocol::OK;	
  inod_info ii(attr);
  ii.dirty = true;
  inods_map[eid] = ii;
  return ret;
}

extent_protocol::status
extent_client::unlink(extent_protocol::extentid_t p_eid, std::string name){
  ScopedLock ml(&m_);
  extent_protocol::status ret = extent_protocol::OK;
  int r;
  extent_protocol::attr attr;
  string parent_buf;

  // Get parent attribute from extent_server
  if(inods_map.find(p_eid) == inods_map.end()){
    ret = cl->call(extent_protocol::getattr, p_eid, attr);
    if(ret == extent_protocol::OK){
      inod_info p_ii(attr);
      inods_map[p_eid] = p_ii;
    }else
      return extent_protocol::NOENT;
  }

  // Get parent contents from extent_server
  if(dirs_map.find(p_eid) == dirs_map.end()){
    ret = cl->call(extent_protocol::get, p_eid, parent_buf);
    if(ret == extent_protocol::OK){
      contents p_content;
      convertToContents(p_content, parent_buf);
      dir_info p_di(p_content);
      dirs_map[p_eid] = p_di;
    }else
      return extent_protocol::NOENT;
  }

  // search parent inode
  extent_protocol::attr parent_attr = inods_map[p_eid].attr;
    
  // Get Parent directory's content
  contents content = dirs_map[p_eid].content;

  // Search file name in the parent directory's content
  contents::iterator c_it = content.find(name);
  if(c_it == content.end()){
    // Modify parent's attribute
    return extent_protocol::NOENT;
  }else{
    // free file's extent
    extent_protocol::extentid_t obj_id = c_it->second;
    remove_list.push_back(obj_id);

    if(obj_id & 0x80000000){   // File
      content.erase(name);
      dirs_map[p_eid].content = content;
      files_map.erase(obj_id);      
      inods_map.erase(obj_id);
      
      // Modify parent's attribute
      parent_attr.mtime = parent_attr.ctime = (int)time(NULL);
      inods_map[p_eid].attr = parent_attr;
      inods_map[p_eid].dirty = true;
      return extent_protocol::OK;

    }else{                       // Directory
      dirs_map.erase(obj_id);
      inods_map.erase(obj_id);
      // Modify parent's attribute
      parent_attr.mtime = parent_attr.ctime = (int)time(NULL);
      inods_map[p_eid].attr = parent_attr;
      inods_map[p_eid].dirty = true;
      return extent_protocol::OK;
    }
  }
  return ret;
}

extent_protocol::status
extent_client::checkDupInum(extent_protocol::extentid_t eid){
  extent_protocol::status ret = extent_protocol::OK;
  int r;

  if(inods_map.find(eid) != inods_map.end())
    ret = extent_protocol::DUPINUM;
  else
    ret = cl->call(extent_protocol::checkDupInum, eid, r);
  return ret;
}

extent_protocol::status
extent_client::flush(extent_protocol::extentid_t eid)
{
  ScopedLock ml(&m_);
  extent_protocol::status ret;

  if(eid & 0x80000000){  // File
    file_info fi = files_map[eid];    
    int r;
    if(inods_map[eid].dirty)
      cl->call(extent_protocol::write, fi.p_id, eid, fi.buf, r);
    files_map.erase( files_map.find(eid) );
    inods_map.erase( inods_map.find(eid) );
  }else{                 // Directory
    dir_info di = dirs_map[eid];
    string str = convertToString(di.content);
    int r;
    if(inods_map[eid].dirty)
      cl->call(extent_protocol::put, di.p_id, eid, str, r);
    dirs_map.erase( dirs_map.find(eid) );
    inods_map.erase( inods_map.find(eid) );
  }

  // remove list
  for(int i=0; i<remove_list.size(); i++){
    int r;
    cl->call(extent_protocol::remove, remove_list[i], r);
  }
  remove_list.clear();

  return extent_protocol::OK;
}

std::string
extent_client::convertToString(contents& content){
  std::string str;
  contents::iterator c_it;
  for(c_it = content.begin(); c_it!=content.end(); c_it++){
    std::stringstream ss;
    ss << c_it->first << ":" << c_it->second << "\n";
    str.append(ss.str());
  }
  return str;
}

void
extent_client::convertToContents(contents& content, string str){
  std::stringstream ss(str);
  std::string temp;
  while(ss >> temp){
    int cur = temp.find(":");
    std::istringstream ist(temp.substr(cur+1));
    unsigned long long finum;
    ist >> finum;
    content[temp.substr(0, cur)] = finum;
  }
}

