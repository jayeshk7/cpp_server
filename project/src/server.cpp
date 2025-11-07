#include <stdlib.h>
#include <iostream>
#include <algorithm>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <string>
#include <atomic>
#include <libpq-fe.h>
#include "httplib.h"
#include "DB.h"

using namespace httplib;

Server svr;
std::unordered_map<std::string, std::string> kv_cache;
std::vector<std::string> fifo_queue;
std::mutex cache_mutex, DB_mutex;
std::atomic<int> cache_hits = 0;

void hihandler(const Request &req, Response &res) {
  res.set_content("hmm..hi... wat u want", "text/plain");
}

void readHandler(const Request &req, Response &res) {

  // std::string key = req.get_param_value("key");
  std::string key = req.path_params.at("key");
  // printf("printing key in readhandler = %s", key.c_str());

  cache_mutex.lock();
  auto iter = kv_cache.find(key);
  
  if(iter != kv_cache.end()) {
    // CACHE HIT
    cache_hits += 1;
    if(cache_hits % 100000 == 0)
      std::cout << "DEBUG >> cache hits " << cache_hits << std::endl;

    res.set_content(iter->second, "text/plain");
    res.status = StatusCode::OK_200;
    cache_mutex.unlock();
    return;
  }
  else {
    // CACHE MISS
    cache_mutex.unlock();
    PGconn *conn = connectDB();

    const char *query = "select key,value from mytable where key = $1";
    const char *params[] = {key.c_str()};

    PGresult *data = PQexecParams(conn, query, 1, NULL, params, NULL, NULL, 0);

    ExecStatusType response_status = PQresultStatus(data);

    if (response_status != PGRES_TUPLES_OK) {

      std::cout << "Error while executing the query: " << PQerrorMessage(conn) << std::endl;

      res.set_content(PQerrorMessage(conn), "text/plain");
      res.status = StatusCode::InternalServerError_500;
    }
    else {

      int rows = PQntuples(data);

      if(rows == 0) {
        res.set_content("This key does not exist. Please create first.", "text/plain");
        res.status = StatusCode::NotFound_404;
      }
      else {
        // we will not have >1 value for a key so no need for a loop to fetch query result
        std::string response_value = PQgetvalue(data, 0, 1); // col 1

        cache_mutex.lock();
        if(kv_cache.size() < 500) {
          kv_cache[key] = response_value;
          fifo_queue.push_back(key);
        }
        else {
          // using fcfs to find which key to evict
          // experiment with lru next
          std::string key_to_evict = fifo_queue.front();
          
          kv_cache.erase(key_to_evict);
          fifo_queue.erase(fifo_queue.begin());

          kv_cache[key] = response_value;
          fifo_queue.push_back(key);
        }
        cache_mutex.unlock();

        // std::cout << response_value << std::endl;
        res.set_content(response_value, "text/plain");
        res.status = StatusCode::OK_200;
      }
    }

    PQclear(data);
    PQfinish(conn);
  }
}

void createHandler(const Request &req, Response &res) {
  
  std::string key, value;
  if(req.has_param("key") && req.has_param("value")) {
    key = req.get_param_value("key");
    value = req.get_param_value("value");
  }
  else {
    res.status = StatusCode::BadRequest_400;
    res.set_content("No key, value pair sent to create.", "text/plain");
    return;
  }

  PGconn *conn = connectDB();

  // DB_mutex.lock();
  const char *query = "insert into mytable (key, value) values ($1, $2) on conflict (key) do update set value = $2";
  const char *params[] = {key.c_str(), value.c_str()};

  PGresult *data = PQexecParams(conn, query, 2, NULL, params, NULL, NULL, 0);

  ExecStatusType response_status = PQresultStatus(data);

  if (response_status != PGRES_COMMAND_OK) {

    std::cout << "INSERT command failed: " << PQerrorMessage(conn) << std::endl;
    res.set_content(PQerrorMessage(conn), "text/plain");
    res.status = StatusCode::InternalServerError_500;
  }
  else {
    std::cout << "Record inserted successfully" << std::endl;
    res.set_content("Added to DB and cache", "text/plain");
    res.status = StatusCode::Created_201;

    cache_mutex.lock();

    auto queue_iter = std::find(fifo_queue.begin(), fifo_queue.end(), key);
    bool key_in_queue = queue_iter != fifo_queue.end() ? true : false;
    bool key_in_cache = kv_cache.find(key) != kv_cache.end() ? true : false;

    if(kv_cache.size() < 500 || key_in_cache) {
      kv_cache[key] = value;
      if (!key_in_queue) {
        fifo_queue.push_back(key);
      }
      else {
        fifo_queue.erase(queue_iter);
        fifo_queue.push_back(key);
      }
    }
    else {
      std::string key_to_evict = fifo_queue.front();

      fifo_queue.erase(fifo_queue.begin());
      kv_cache.erase(key_to_evict);

      kv_cache[key] = value;
      fifo_queue.push_back(key);
    }
    cache_mutex.unlock();
  }

  // DB_mutex.unlock();
  PQclear(data);
  PQfinish(conn);
}

void deleteHandler(const Request &req, Response &res) {
  auto key = req.path_params.at("key");

  PGconn *conn = connectDB();

  // DB_mutex.lock();
  const char *query = "delete from mytable where key = $1";
  const char *params[] = {key.c_str()};

  PGresult *data = PQexecParams(conn, query, 1, NULL, params, NULL, NULL, 0);
  ExecStatusType response_status = PQresultStatus(data);

  if (response_status != PGRES_COMMAND_OK) {

    std::cout << "DELETE command failed: " << PQerrorMessage(conn) << std::endl;
    res.set_content(PQerrorMessage(conn), "text/plain");
    res.status = StatusCode::InternalServerError_500;
  }
  else {
    std::cout << "Record deleted successfully" << std::endl;
    res.set_content("Key deleted from DB and cache", "text/plain");
    res.status = StatusCode::OK_200;
    
    cache_mutex.lock();
    auto cache_iter = kv_cache.find(key);
    auto queue_iter = std::find(fifo_queue.begin(), fifo_queue.end(), key);

    if(cache_iter != kv_cache.end()) {
      kv_cache.erase(key);
    }
    if(queue_iter != fifo_queue.end()) {
      fifo_queue.erase(queue_iter);
    }
    cache_mutex.unlock();
  }

  // DB_mutex.unlock();
  PQclear(data);
  PQfinish(conn);
}

int main() {

  svr.Get("/", hihandler);

  svr.Get("/read/:key", readHandler);

  svr.Post("/create", createHandler);

  svr.Delete("/delete/:key", deleteHandler);

  svr.listen("127.0.0.1", 8080);
}

// TODO
// 1. Add access logging (to measure request service time) and error logging
// 2. Add error handler and exception handler 
// 3. Experiment with different threadpool size (default = 8) and max_queued_requests. you can take it as user input
// 4. study RAII. using mutex.lock()/unlock() can fail if the code within the lock throws an exception. then the lock will never be yielded => deadlock


// each api opening a new connection to DB is slow. for each thing you will have to do tcp + auth. so connection pooling is better. number of connections can be == number of threads in my design. experiment with pooling size.
