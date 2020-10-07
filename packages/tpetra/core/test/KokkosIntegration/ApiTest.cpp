
#include <string>
#include <map>
#include <list>
#include "ApiTest.h"

ApiTest::ApiTest() { }

ApiTest::~ApiTest() { }

int ApiTest::setExpectations(std::map<std::string, int> &exp) {
  for (std::map<std::string, int>::iterator it = exp.begin();
       it != exp.end(); it++) {
    std::map<std::string, std::pair<int, int> >::iterator found = counter.find(it->first);
    if (found != counter.end()) {
      found->second.second = it->second;
    } else {
      return -1;
    }
  }
  return 0;
}

void ApiTest::map_zero() {
  for (std::list<std::string>::iterator it = ApiTest::funcs.begin(); it != ApiTest::funcs.end(); it++) {
    std::map<std::string, std::pair<int, int> >::iterator found = counter.find(*it);
    if (found != counter.end()) {
      found->second = std::make_pair(0, 0);
    } else {
      counter.insert(std::make_pair(*it, std::make_pair(0, 0)));
    }
  }
}

void ApiTest::pincr(std::string key, int val) {
  std::map<std::string, std::pair<int, int> >::iterator it = counter.find(key);
  if (it == counter.end()) {
    counter.insert(std::make_pair(key, std::make_pair(1, 0)));
  } else {
    it->second.first += val;
  }
}

void ApiTest::incr(std::string key) {
  pincr(key, 1);
}

void ApiTest::printAll() {
  fprintf (stderr, "**************** cuda call analysis ****************\n"); 
  fprintf (stderr, "call                                           times\n");
  for (std::map<std::string, std::pair<int, int> >::iterator it = counter.begin(); 
       it != counter.end(); it++) {
    fprintf(stderr, "%-50s %d\t%d\n", it->first.c_str(), it->second.first, it->second.second);
  }
  fprintf (stderr, "**************** cuda call analysis ****************\n"); 
}
