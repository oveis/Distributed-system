// extent client interface.

#ifndef extent_client_h
#define extent_client_h

#include <string>
#include "extent_protocol.h"
#include "rpc.h"

class extent_client {
 private:
  rpcc *cl;

 public:
  extent_client(std::string dst);

  extent_protocol::status get(extent_protocol::extentid_t eid, 
			      std::string &buf);
  extent_protocol::status getattr(extent_protocol::extentid_t eid, 
				  extent_protocol::attr &a);
  extent_protocol::status put(extent_protocol::extentid_t p_eid, extent_protocol::extentid_t eid, std::string buf);
  extent_protocol::status remove(extent_protocol::extentid_t eid);
  extent_protocol::status write(extent_protocol::extentid_t eid, std::string buf, long long unsigned int, size_t);
  extent_protocol::status read(extent_protocol::extentid_t eid, size_t, long long unsigned int, std::string&);
  extent_protocol::status setattr(extent_protocol::extentid_t eid, extent_protocol::attr);
  extent_protocol::status unlink(extent_protocol::extentid_t p_eid, std::string);
};

#endif 

