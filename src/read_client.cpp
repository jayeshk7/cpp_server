#include <iostream>
#include <cstdlib>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <random>
#include <string>
#include "httplib.h"

using namespace httplib;

int NUM_THREADS = 8;
int TEST_DURATION = 120; // seconds

std::atomic<int> total_success_count(0);
std::atomic<int> total_error_count(0);

void worker_thread(std::chrono::steady_clock::time_point global_start) {

  Client cli("127.0.0.1", 8080);
  cli.set_connection_timeout(10, 0); // 10-second timeout
                                     
  // per-thread RNG
  std::random_device rd;
  std::mt19937 rng(rd());

  const std::string keys[] = {"yo","yo2","yo3","yo4","yo5","yo6","yo7","yo8","yo9","yo10"};
  std::uniform_int_distribution<int> dist(0, 9);

  while (true) {

    // Stop when global duration reached so threads finish roughly together
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - global_start);
    if (elapsed.count() >= TEST_DURATION) break;

    std::string key = keys[dist(rng)];

    std::string path = std::string("/read/") + key;

    if (auto res = cli.Get(path.c_str())) {
      if (res->status == 200) {
        total_success_count.fetch_add(1, std::memory_order_relaxed);
      } else {
        total_error_count.fetch_add(1, std::memory_order_relaxed);
      }
    } else {
      total_error_count.fetch_add(1, std::memory_order_relaxed);
    }
  }
}

int main(int argc, char *argv[]) {

  if(argc >= 3) {
    NUM_THREADS = atoi(argv[1]);
    TEST_DURATION = atoi(argv[2]);
  }

  std::cout << "Starting load test with " << NUM_THREADS
    << " concurrent users for " << TEST_DURATION << " seconds...\n";

  std::vector<std::thread> threads;
  auto test_start = std::chrono::steady_clock::now();

  // spawn worker threads (pass the global start time so they stop together)
  for (int i = 0; i < NUM_THREADS; ++i) {
    threads.emplace_back(worker_thread, test_start);
  }

  for (auto &t : threads) t.join();

  auto test_end = std::chrono::steady_clock::now();
  double duration_sec = std::chrono::duration_cast<std::chrono::duration<double>>(test_end - test_start).count();

  int successes = total_success_count.load();
  int errors = total_error_count.load();
  double rps = (duration_sec > 0.0) ? (successes / duration_sec) : 0.0;

  std::cout << "\n--- Test Finished ---\n";
  std::cout << "Total Requests:     " << (successes + errors) << "\n";
  std::cout << "Successful:         " << successes << "\n";
  std::cout << "Errors:             " << errors << "\n";
  std::cout << "Duration:           " << duration_sec << " s\n";
  std::cout << "Throughput (RPS):   " << rps << "\n";

  return 0;
}
