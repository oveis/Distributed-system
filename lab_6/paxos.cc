#include "paxos.h"
#include "handle.h"
// #include <signal.h>
#include <stdio.h>
#include "tprintf.h"
#include "lang/verify.h"



// This module implements the proposer and acceptor of the Paxos
// distributed algorithm as described by Lamport's "Paxos Made
// Simple".  To kick off an instance of Paxos, the caller supplies a
// list of nodes, a proposed value, and invokes the proposer.  If the
// majority of the nodes agree on the proposed value after running
// this instance of Paxos, the acceptor invokes the upcall
// paxos_commit to inform higher layers of the agreed value for this
// instance.


bool
operator> (const prop_t &a, const prop_t &b)
{
  return (a.n > b.n || (a.n == b.n && a.m > b.m));
}

bool
operator>= (const prop_t &a, const prop_t &b)
{
  return (a.n > b.n || (a.n == b.n && a.m >= b.m));
}

std::string
print_members(const std::vector<std::string> &nodes)
{
  std::string s;
  s.clear();
  for (unsigned i = 0; i < nodes.size(); i++) {
    s += nodes[i];
    if (i < (nodes.size()-1))
      s += ",";
  }
  return s;
}

bool isamember(std::string m, const std::vector<std::string> &nodes)
{
  for (unsigned i = 0; i < nodes.size(); i++) {
    if (nodes[i] == m) return 1;
  }
  return 0;
}

bool
proposer::isrunning()
{
  bool r;
  ScopedLock ml(&pxs_mutex);
  r = !stable;
  return r;
}

// check if the servers in l2 contains a majority of servers in l1
bool
proposer::majority(const std::vector<std::string> &l1, 
		const std::vector<std::string> &l2)
{
  unsigned n = 0;

  for (unsigned i = 0; i < l1.size(); i++) {
    if (isamember(l1[i], l2))
      n++;
  }

  //test
  file << "member: [";
  for(unsigned i=0; i<l1.size(); i++)
    file << l1[i] << ", ";
  file << "] , accepter: [";
  for(unsigned i=0; i<l2.size(); i++)
    file << l2[i] << ", ";
  file << "]\n";

  return n >= (l1.size() >> 1) + 1;
}

proposer::proposer(class paxos_change *_cfg, class acceptor *_acceptor, 
		   std::string _me)
  : cfg(_cfg), acc (_acceptor), me (_me), break1 (false), break2 (false), 
    stable (true)
{
  VERIFY (pthread_mutex_init(&pxs_mutex, NULL) == 0);
  my_n.n = 0;
  my_n.m = me;

  string file_name = "proposer_log_" + me + ".txt";
  file.open(file_name.c_str());
}

proposer::~proposer(){
  file.close();
}

void
proposer::setn()
{
  my_n.n = acc->get_n_h().n + 1 > my_n.n + 1 ? acc->get_n_h().n + 1 : my_n.n + 1;
}

// This method is used to get the members of the current view (nodes) to agree to a value v.
bool
proposer::run(int instance, std::vector<std::string> cur_nodes, std::string newv)
{
  std::vector<std::string> accepts;
  std::vector<std::string> nodes;
  std::string v;
  bool r = false;

  ScopedLock ml(&pxs_mutex);
  tprintf("start: initiate paxos for %s w. i=%d v=%s stable=%d\n",
	 print_members(cur_nodes).c_str(), instance, newv.c_str(), stable);
  if (!stable) {  // already running proposer?
    tprintf("proposer::run: already running\n");
    return false;
  }
  
  stable = false;
  setn();   // choose n, unique and higher than any n seen so far
  accepts.clear();
  v.clear();

  // Request Prepare
  if (prepare(instance, accepts, cur_nodes, v)) {

    // Get majority from 'Prepare' request
    if (majority(cur_nodes, accepts)) {
      tprintf("paxos::manager: received a majority of prepare responses\n");

      if (v.size() == 0)
	v = newv;

      breakpoint1();

      nodes = accepts;
      accepts.clear();
      accept(instance, accepts, nodes, v);

      // Get majority from 'Accept' request
      if (majority(cur_nodes, accepts)) {
	tprintf("paxos::manager: received a majority of accept responses\n");

	breakpoint2();
	decide(instance, accepts, v);

	r = true;
      } else {
	tprintf("paxos::manager: no majority of accept responses\n");
      }
    } else {  // Cannot get majority from 'Prepare' request
      tprintf("paxos::manager: no majority of prepare responses\n");
    }
  } else {
    tprintf("paxos::manager: prepare is rejected %d\n", stable);
  }
  stable = true;
  return r;
}

// proposer::run() calls prepare to send prepare RPCs to nodes
// and collect responses. if one of those nodes
// replies with an oldinstance, return false.
// otherwise fill in accepts with set of nodes that accepted,
// set v to the v_a with the highest n_a, and return true.
bool
proposer::prepare(unsigned instance, std::vector<std::string> &accepts, 
         std::vector<std::string> nodes,
         std::string &v)
{
  file << "prepare start" << endl;
  // You fill this in for Lab 6
  // Note: if got an "oldinstance" reply, commit the instance using
  // acc->commit(...), and return false.

  // send prepare(instance, n) to all servers including self
  //ScopedLock ml(&pxs_mutex);
  paxos_protocol::preparearg a;
  a.instance = instance;
  a.n = my_n;

  prop_t highest_n_a = a.n;
  v = acc->value(instance);
  for(int i=0; i<nodes.size(); i++){
    std::string node = nodes[i];
    handle h(node);
    rpcc *cl = h.safebind();

    if(cl){
      paxos_protocol::prepareres r;
      paxos_protocol::status ret;
      std::string src;
      ret = cl->call(paxos_protocol::preparereq, src, a, r, rpcc::to(1000));
      
      if(ret == paxos_protocol::OK){
	if(r.oldinstance){
	  acc->commit(instance, r.v_a);
	  return false;
	}else if(r.accept){
	  accepts.push_back(node);
	  if(r.n_a > highest_n_a){
	    highest_n_a = r.n_a;
	    v = r.v_a;
	  }
	}
      }
    }
  }
  
  file << "prepare end" << endl;
  return true;
}

// run() calls this to send out accept RPCs to accepts.
// fill in accepts with list of nodes that accepted.
void
proposer::accept(unsigned instance, std::vector<std::string> &accepts,
        std::vector<std::string> nodes, std::string v)
{
  //ScopedLock ml(&pxs_mutex);
  file << "accept start" << endl;
  std::string src;
  paxos_protocol::acceptarg a;
  a.instance = instance;
  a.n = my_n;
  a.v = v;

  // You fill this in for Lab 6
  for(int i=0; i<nodes.size(); i++){
    std::string node = nodes[i];
    handle h(node);
    rpcc *cl = h.safebind();
    paxos_protocol::status ret;

    if(cl){
      bool r;
      ret = cl->call(paxos_protocol::acceptreq, src, a, r, rpcc::to(1000));
      if(ret == paxos_protocol::OK){
	if(r)
	  accepts.push_back(node);
      }
    }
  }
  file << "accept end" << endl;
}

void
proposer::decide(unsigned instance, std::vector<std::string> accepts, 
	      std::string v)
{
  //ScopedLock ml(&pxs_mutex);
  file << "decide start " << endl;

  // You fill this in for Lab 6
  paxos_protocol::status ret;
  std::string src;
  int r;
  paxos_protocol::decidearg a;
  a.instance = instance;
  a.v = v;

  for(int i=0; i<accepts.size(); i++){
    std::string accept = accepts[i];
    handle h(accept);
    rpcc *cl = h.safebind();
    if(cl){
      ret = cl->call(paxos_protocol::decidereq, src, a, r, rpcc::to(1000));
      if(ret == paxos_protocol::OK){
	// r 

      }
    }
  }
}

acceptor::acceptor(class paxos_change *_cfg, bool _first, std::string _me, 
	     std::string _value)
  : cfg(_cfg), me (_me), instance_h(0)
{
  VERIFY (pthread_mutex_init(&pxs_mutex, NULL) == 0);

  n_h.n = 0;
  n_h.m = me;
  n_a.n = 0;
  n_a.m = me;
  v_a.clear();

  l = new log (this, me);

  if (instance_h == 0 && _first) {
    values[1] = _value;
    l->loginstance(1, _value);
    instance_h = 1;
  }

  pxs = new rpcs(atoi(_me.c_str()));
  pxs->reg(paxos_protocol::preparereq, this, &acceptor::preparereq);
  pxs->reg(paxos_protocol::acceptreq, this, &acceptor::acceptreq);
  pxs->reg(paxos_protocol::decidereq, this, &acceptor::decidereq);

  string file_name = "acceptor_log_" + me + ".txt";
  file.open(file_name.c_str());
}

acceptor::~acceptor(){
  file.close();
}

paxos_protocol::status
acceptor::preparereq(std::string src, paxos_protocol::preparearg a,
    paxos_protocol::prepareres &r)
{
  file << "preparereq start" << endl;
  // You fill this in for Lab 6
  // Remember to initialize *BOTH* r.accept and r.oldinstance appropriately.
  // Remember to *log* the proposal if the proposal is accepted.
  ScopedLock ml(&pxs_mutex);
  if(a.instance <= instance_h){  // we got proposal with old number
    r.oldinstance = true;
    r.accept = false;
    r.n_a = a.n;
    r.v_a = value(a.instance);
  }else if(a.n > n_h){
    n_h = a.n;
    l->logprop(n_h); 
    r.oldinstance = false;
    r.accept = true;
    r.n_a = n_a;
    r.v_a = v_a;
  }else{
    r.oldinstance = false;
    r.accept = false;
  }

  file << "preparereq end" << endl;
  return paxos_protocol::OK;
}

// the src argument is only for debug purpose
paxos_protocol::status
acceptor::acceptreq(std::string src, paxos_protocol::acceptarg a, bool &r)
{
  file << "acceptreq start" << endl;

  // You fill this in for Lab 6
  // Remember to *log* the accept if the proposal is accepted.
  ScopedLock ml(&pxs_mutex);
  if(a.n >= n_h){
    l->logaccept(a.n, a.v);
    l->logaccept(n_a, v_a);
    n_a = a.n;
    v_a = a.v;
    r = true;
  }else{
    r = false;
  }
  file << "acceptreq end" << endl; 

  return paxos_protocol::OK; 
}

// the src argument is only for debug purpose
paxos_protocol::status
acceptor::decidereq(std::string src, paxos_protocol::decidearg a, int &r)
{
  file << "decidereq start" << endl;

  ScopedLock ml(&pxs_mutex);
  tprintf("decidereq for accepted instance %d (my instance %d) v=%s\n", 
	 a.instance, instance_h, v_a.c_str());

  if (a.instance == instance_h + 1) {
    VERIFY(v_a == a.v);
    commit_wo(a.instance, v_a);
  } else if (a.instance <= instance_h) {
    // we are ahead ignore.
  } else {
    // we are behind
    VERIFY(0);
  }

  file << "decidereq end" << endl;
  return paxos_protocol::OK;
}

void
acceptor::commit_wo(unsigned instance, std::string value)
{
  file << "commit_wo start" << endl;
  //assume pxs_mutex is held
  tprintf("acceptor::commit: instance=%d has v= %s\n", instance, value.c_str());

  if (instance  > instance_h) {
    l->loginstance(instance, value);
    tprintf("commit: highestaccepteinstance = %d\n", instance);
    values[instance] = value;
    instance_h = instance;
    n_h.n = 0;
    n_h.m = me;
    n_a.n = 0;
    n_a.m = me;
    v_a.clear();
    if (cfg) {
      pthread_mutex_unlock(&pxs_mutex);
      cfg->paxos_commit(instance, value);
      pthread_mutex_lock(&pxs_mutex);
      file << "commit_wo [" << instance << ": " << value << "]" << endl;
    }
  }
  file << "commit_wo end" << endl;
}

void
acceptor::commit(unsigned instance, std::string value)
{
  file << "commit start" << endl;
  ScopedLock ml(&pxs_mutex);
  commit_wo(instance, value);
  file << "commit end" << endl;
}

std::string
acceptor::dump()
{
  return l->dump();
}

void
acceptor::restore(std::string s)
{
  l->restore(s);
  l->logread();
}



// For testing purposes

// Call this from your code between phases prepare and accept of proposer
void
proposer::breakpoint1()
{
  if (break1) {
    tprintf("Dying at breakpoint 1!\n");
    exit(1);
  }
}

// Call this from your code between phases accept and decide of proposer
void
proposer::breakpoint2()
{
  if (break2) {
    tprintf("Dying at breakpoint 2!\n");
    exit(1);
  }
}

void
proposer::breakpoint(int b)
{
  if (b == 3) {
    tprintf("Proposer: breakpoint 1\n");
    break1 = true;
  } else if (b == 4) {
    tprintf("Proposer: breakpoint 2\n");
    break2 = true;
  }
}
