#include <boost/program_options.hpp>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>
#include <sstream>

struct Config {
  std::vector<int> criticalBatteryLevels;
  std::vector<int> lowBatteryLevels;
  int batteryNumber;

  Config() : batteryNumber(1) {} // Default values
};

std::string getBatteryStatus(int &capacity, const std::vector<int> &criticalLevels,
                             const std::vector<int> &lowLevels, int batteryNumber) {
  std::string batteryPath = "/sys/class/power_supply/BAT" +
                            std::to_string(batteryNumber) + "/capacity";
  std::ifstream capacityFile(batteryPath);

  if (capacityFile.is_open()) {
    capacityFile >> capacity;
  }

  std::string status = "Normal";
  for (int level : criticalLevels) {
    if (capacity < level) {
      status = "Critical";
      break;
    }
  }

  if (status == "Normal") {
    for (int level : lowLevels) {
      if (capacity < level) {
        status = "Low";
        break;
      }
    }
  }

  return status;
}

void sendNotification(const std::string &message) {
  std::string command = "notify-send -u critical 'Battery Notification' '" + message + "'";
  system(command.c_str());
}

void sendBatteryFull() {
  system("notify-send -u normal 'Battery Full' 'Battery at 100%'");
}

bool readLockState(const std::string &lockFilePath) {
  std::ifstream lockFile(lockFilePath);
  bool lockState = false;
  if (lockFile.is_open()) {
    lockFile >> lockState;
  }
  return lockState;
}

void writeLockState(const std::string &lockFilePath, bool lockState) {
  std::ofstream lockFile(lockFilePath, std::ios_base::trunc);
  lockFile << lockState;
}

std::vector<int> parseBatteryLevels(const std::string &levelsStr) {
  std::vector<int> levels;
  std::stringstream ss(levelsStr);
  std::string level;
  while (std::getline(ss, level, ',')) {
    levels.push_back(std::stoi(level));
  }
  return levels;
}

Config readConfig(const std::string &configFilePath) {
  Config config;
  boost::program_options::options_description desc("Allowed options");
  desc.add_options()
    ("critical_bat", boost::program_options::value<std::string>()->default_value("5,10,15"), "Critical battery levels")
    ("low_bat", boost::program_options::value<std::string>()->default_value("20,30"), "Low battery levels")
    ("bat", boost::program_options::value<int>(&config.batteryNumber), "Battery number");

  boost::program_options::variables_map vm;
  std::ifstream configFile(configFilePath);
  if (configFile) {
    boost::program_options::store(
        boost::program_options::parse_config_file(configFile, desc), vm);
    boost::program_options::notify(vm);
  }

  config.criticalBatteryLevels = parseBatteryLevels(vm["critical_bat"].as<std::string>());
  config.lowBatteryLevels = parseBatteryLevels(vm["low_bat"].as<std::string>());

  return config;
}

int main() {
  const char *homeDir = getenv("HOME");
  std::string logFilePath = std::string(homeDir) + "/.cache/.battery_monitor.log";
  std::string lockFilePath = std::string(homeDir) + "/.cache/.battery_monitor.lock";
  std::string configFilePath = std::string(homeDir) + "/.config/lowkey/config";
  std::ofstream logFile(logFilePath, std::ios_base::trunc);

  Config config = readConfig(configFilePath);

  const std::vector<int> &criticalLevels = config.criticalBatteryLevels;
  const std::vector<int> &lowLevels = config.lowBatteryLevels;
  int batteryNumber = config.batteryNumber;
  bool lockState = readLockState(lockFilePath);

  int capacity = 0;
  std::string status = getBatteryStatus(capacity, criticalLevels, lowLevels, batteryNumber);

  logFile << "Battery Capacity: " << capacity << "%, Status: " << status << "\n";
  logFile.flush();

  if (status == "Critical" && !lockState) {
    sendNotification("Critical battery level reached. Charge soon.");
    lockState = true;
    writeLockState(lockFilePath, lockState);
  } else if (status == "Low" && !lockState) {
    sendNotification("Low battery level reached. Consider charging.");
    lockState = true;
    writeLockState(lockFilePath, lockState);
  } else if (capacity >= *std::min_element(criticalLevels.begin(), criticalLevels.end()) && lockState && capacity < 100) {
    lockState = false;
    writeLockState(lockFilePath, lockState);
  } else if (capacity == 100 && !lockState) {
    sendBatteryFull();
    lockState = true;
    writeLockState(lockFilePath, lockState);
  }

  std::cout << "Battery Capacity: " << capacity << "%, Status: " << status << std::endl;
  std::cout << "lockState: " << lockState << std::endl;
  std::cout << "Critical Levels: ";
  for (int level : criticalLevels) {
    std::cout << level << " ";
  }
  std::cout << std::endl;
  std::cout << "Low Levels: ";
  for (int level : lowLevels) {
    std::cout << level << " ";
  }
  std::cout << std::endl;
  std::cout << "Battery Number: " << batteryNumber << std::endl;

  return 0;
}
