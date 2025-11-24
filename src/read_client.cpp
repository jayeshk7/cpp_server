#include <iostream>
#include <cstdlib>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <random>
#include <string>
#include <fstream>
#include <sstream>
#include "httplib.h"

using namespace httplib;

int NUM_THREADS = 1;
int TEST_DURATION = 120; // seconds

std::atomic<int> total_success_count(0);
std::atomic<int> total_error_count(0);
std::atomic<long long> total_response_time(0);

struct SysStats {
    long long cpu_active;
    long long cpu_total;
    long long disk_busy_time; // tracking time spent doing I/Os (ms)
};

SysStats get_server_load_stats() {
    SysStats stats = {0, 0, 0};

    std::ifstream cpu_file("/proc/stat");
    if (cpu_file.is_open()) {
        std::string line;
        while (std::getline(cpu_file, line)) {
            std::stringstream ss(line);
            std::string label;
            ss >> label;

            // filtering specifically for cpu6 and cpu7 since server is pinned to those cores
            if (label == "cpu6" || label == "cpu7") {
                long long user, nice, system, idle, iowait, irq, softirq, steal;
                ss >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;

                long long active = user + nice + system + irq + softirq + steal;
                long long total = active + idle + iowait;

                stats.cpu_active += active;
                stats.cpu_total += total;
            }
        }
        cpu_file.close();
    }

    // reading disk util for sdb only since that is where the writes are happening
    std::ifstream disk_file("/proc/diskstats");
    if (disk_file.is_open()) {
        std::string line;
        while (std::getline(disk_file, line)) {
            std::stringstream ss(line);
            int major, minor;
            std::string name;
            long long r_completed, r_merged, r_sectors, r_time;
            long long w_completed, w_merged, w_sectors, w_time;
            long long ios_in_prog, io_ticks; // fields needed for utilization calculation

            // Read standard fields (1-11)
            ss >> major >> minor >> name 
               >> r_completed >> r_merged >> r_sectors >> r_time
               >> w_completed >> w_merged >> w_sectors >> w_time
               // Read fields 12 & 13 (io_ticks is time spent doing I/Os in ms)
               >> ios_in_prog >> io_ticks;

            if (name == "sdb") {
                stats.disk_busy_time += io_ticks;
            }
        }
        disk_file.close();
    }

    return stats;
}

void worker_thread(std::chrono::steady_clock::time_point global_start) {

  Client cli("127.0.0.1", 8080);
  cli.set_connection_timeout(10, 0); 
                                     
  std::random_device rd;
  std::mt19937 rng(rd());

  const std::string keys[] = {"yo","yo2","yo3","yo4","yo5","yo6","yo7","yo8","yo9","yo10"};
  std::uniform_int_distribution<int> dist(0, 9);

  while (true) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - global_start);
    if (elapsed.count() >= TEST_DURATION) break;

    std::string key = keys[dist(rng)];
    std::string path = std::string("/read/") + key;

    auto request_start = std::chrono::steady_clock::now();

    if (auto res = cli.Get(path.c_str())) {
      auto request_end = std::chrono::steady_clock::now();
      auto response_time = std::chrono::duration_cast<std::chrono::milliseconds>(request_end - request_start).count();

      if (res->status == 200) {
        total_success_count.fetch_add(1, std::memory_order_relaxed);
        total_response_time.fetch_add(response_time, std::memory_order_relaxed);
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
  
  // --- START SNAPSHOT ---
  SysStats start_stats = get_server_load_stats();
  
  auto test_start = std::chrono::steady_clock::now();

  for (int i = 0; i < NUM_THREADS; ++i) {
    threads.emplace_back(worker_thread, test_start);
  }

  for (auto &t : threads) t.join();

  auto test_end = std::chrono::steady_clock::now();
  
  // --- END SNAPSHOT ---
  SysStats end_stats = get_server_load_stats();

  double duration_sec = std::chrono::duration_cast<std::chrono::duration<double>>(test_end - test_start).count();

  int successes = total_success_count.load();
  int errors = total_error_count.load();
  long long total_time_ms = total_response_time.load();

  double rps = (duration_sec > 0.0) ? (successes / duration_sec) : 0.0;
  double avg_response_time_ms = (successes > 0) ? (static_cast<double>(total_time_ms) / successes) : 0.0;

  long long cpu_active_delta = end_stats.cpu_active - start_stats.cpu_active;
  long long cpu_total_delta = end_stats.cpu_total - start_stats.cpu_total;
  double cpu_util = (cpu_total_delta > 0) ? 100.0 * cpu_active_delta / cpu_total_delta : 0.0;

  long long disk_time_delta = end_stats.disk_busy_time - start_stats.disk_busy_time;
  // Utilization = (Busy Time (ms) / Elapsed Time (ms)) * 100
  double disk_util = (duration_sec > 0.0) ? (double)disk_time_delta / (duration_sec * 1000.0) * 100.0 : 0.0;

  std::cout << "\n--- Test Finished ---\n";
  std::cout << "Total Requests:      " << (successes + errors) << "\n";
  std::cout << "Successful:          " << successes << "\n";
  std::cout << "Errors:              " << errors << "\n";
  std::cout << "Duration:            " << duration_sec << " s\n";
  std::cout << "Throughput (RPS):    " << rps << "\n";
  std::cout << "Avg Response Time:   " << avg_response_time_ms << " ms\n";
  
  std::cout << "\n--- Resource Usage ---\n";
  std::cout << "CPU Utilization (Cores 6,7): " << cpu_util << " %\n";
  std::cout << "Disk Utilization (sdb): " << disk_util << " %\n";

  return 0;
}
