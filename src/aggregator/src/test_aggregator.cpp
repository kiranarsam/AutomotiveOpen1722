
#include "AggregatorFactory.hpp"

#include <mutex>
#include <condition_variable>
#include <csignal>
#include <iostream>

std::mutex g_mutex;
std::condition_variable g_cond_var;

bool g_ready = false;

void handleSignal(int sig);

void handleSignal(int sig)
{
  if((sig == SIGINT) || (sig == SIGTERM)) {
    std::cout << "EXITED invoked sigint or sigterm " << std::endl;
    g_ready = true;
    g_cond_var.notify_one();
  }
}

int main() {

  auto callback = [](callback_data &msg) {
    std::cout << "Main data = " << msg.name << std::endl;
    std::cout << "Main data can_type = " << static_cast<int>(msg.cf.type) << std::endl;
    std::cout << "Main data can_id = " << msg.cf.data.cc.can_id << std::endl;
  };

  DataCallbackHandler handler;
  handler.registerCallback(callback);

  auto aggregator = AggregatorFactory::createCanAggregator();
  std::cout << "Main setDataHandler() " << std::endl;
  aggregator->setDataHandler(std::move(handler));

  std::cout << "Main start() IN " << std::endl;

  aggregator->start();

  std::cout << "Main start() OUT " << std::endl;

  std::signal(SIGINT, handleSignal);
  std::signal(SIGTERM, handleSignal);

  std::unique_lock<std::mutex> lock(g_mutex);
  g_cond_var.wait(lock, [&] { return g_ready; });
  lock.unlock();

  std::cout << "Main stop() IN" << std::endl;
  aggregator->stop();
  std::cout << "Main stop() OUT" << std::endl;

  return 0;
}