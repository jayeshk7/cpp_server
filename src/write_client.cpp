#include <iostream>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <string>
#include "httplib.h"

using namespace httplib;

int NUM_THREADS = 70;
int TEST_DURATION = 300; // seconds

std::atomic<int> total_success_count(0);
std::atomic<int> total_error_count(0);
// This counter ensures every thread gets a unique key
std::atomic<int> global_key_counter(11); // Start creating from 'yo11'

void worker_thread(std::chrono::steady_clock::time_point global_start) {
  Client cli("127.0.0.1", 8080);
  cli.set_connection_timeout(10, 0); // 10-second timeout

  while (1) {

    // Stop when global duration is reached
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - global_start);
    if (elapsed.count() >= TEST_DURATION) {
      break;
    }

    int current_key_index = global_key_counter.fetch_add(1, std::memory_order_relaxed);

    std::string key = "yo" + std::to_string(current_key_index);
    std::string value = "this is the " + std::to_string(current_key_index) + " value";

    Params params;
    params.emplace("key", key);
    params.emplace("value", value);

    if (auto res = cli.Post("/create", params)) {
      // check for 201 created
      if (res->status == httplib::StatusCode::Created_201) {
        total_success_count.fetch_add(1, std::memory_order_relaxed);
      } else {
        total_error_count.fetch_add(1, std::memory_order_relaxed);
      }
    } else {
      // connection failed
      total_error_count.fetch_add(1, std::memory_order_relaxed);
    }
  }
}

int main(int argc, char *argv[]) {

  if(argc >= 3) {
    NUM_THREADS = atoi(argv[1]);
    TEST_DURATION = atoi(argv[2]);
  }

  std::cout << "Starting 'create' load test with " << NUM_THREADS
    << " concurrent users for " << TEST_DURATION << " seconds...\n";

  std::vector<std::thread> threads;
  auto test_start = std::chrono::steady_clock::now();

  for (int i = 0; i < NUM_THREADS; ++i) {
    threads.emplace_back(worker_thread, test_start);
  }

  for (auto &t : threads) {
    t.join();
  }

  auto test_end = std::chrono::steady_clock::now();
  double duration_sec = std::chrono::duration_cast<std::chrono::duration<double>>(test_end - test_start).count();

  int successes = total_success_count.load();
  int errors = total_error_count.load();
  double rps = (duration_sec > 0.0) ? (successes / duration_sec) : 0.0;

  std::cout << "\n--- Test Finished ---\n";
  std::cout << "Total Requests:      " << (successes + errors) << "\n";
  std::cout << "Successful Inserts:  " << successes << "\n";
  std::cout << "Errors:              " << errors << "\n";
  std::cout << "Duration:            " << duration_sec << " s\n";
  std::cout << "Throughput (RPS):    " << rps << "\n";

  return 0;
}
