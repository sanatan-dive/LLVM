#include <cstdint>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <string>

// Data structures for storing profiling information
static std::unordered_map<uint32_t, uint64_t> BlockExecutionCounts;
static std::unordered_map<uintptr_t, uint64_t> LoadAddressCounts;
static std::unordered_map<uintptr_t, uint64_t> StoreAddressCounts;
static std::vector<bool> BranchPredictions;
static std::mutex ProfileMutex;

// Function to increment block execution counter
extern "C" void incrementBlockCounter(uint32_t BlockID) {
  std::lock_guard<std::mutex> Lock(ProfileMutex);
  BlockExecutionCounts[BlockID]++;
}

// Function to profile load operations
extern "C" void profileLoad(void* Address) {
  std::lock_guard<std::mutex> Lock(ProfileMutex);
  uintptr_t Addr = reinterpret_cast<uintptr_t>(Address);
  LoadAddressCounts[Addr]++;
}

// Function to profile store operations
extern "C" void profileStore(void* Address) {
  std::lock_guard<std::mutex> Lock(ProfileMutex);
  uintptr_t Addr = reinterpret_cast<uintptr_t>(Address);
  StoreAddressCounts[Addr]++;
}

// Function to profile branch predictions
extern "C" void profileBranch(uint32_t Taken) {
  std::lock_guard<std::mutex> Lock(ProfileMutex);
  BranchPredictions.push_back(Taken != 0);
}

// Function to write profiling data to a file
extern "C" void writeProfilingData(const char* Filename) {
  std::lock_guard<std::mutex> Lock(ProfileMutex);
  
  std::ofstream OutFile(Filename);
  if (!OutFile.is_open()) {
    std::cerr << "Error: Could not open file " << Filename << " for writing\n";
    return;
  }
  
  // Write block execution counts
  OutFile << "BLOCK_EXECUTION_COUNTS\n";
  for (const auto& Entry : BlockExecutionCounts) {
    OutFile << Entry.first << " " << Entry.second << "\n";
  }
  
  // Write load address counts
  OutFile << "LOAD_ADDRESS_COUNTS\n";
  for (const auto& Entry : LoadAddressCounts) {
    OutFile << Entry.first << " " << Entry.second << "\n";
  }
  
  // Write store address counts
  OutFile << "STORE_ADDRESS_COUNTS\n";
  for (const auto& Entry : StoreAddressCounts) {
    OutFile << Entry.first << " " << Entry.second << "\n";
  }
  
  // Write branch predictions
  OutFile << "BRANCH_PREDICTIONS\n";
  for (size_t i = 0; i < BranchPredictions.size(); ++i) {
    OutFile << (BranchPredictions[i] ? "1" : "0");
    if ((i + 1) % 80 == 0) OutFile << "\n";
  }
  
  OutFile.close();
}

// Constructor to register atexit handler
static void __attribute__((constructor)) InitializeProfiler() {
  std::atexit([]() {
    writeProfilingData("paralldiscover_profile.dat");
  });
}