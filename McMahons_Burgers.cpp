#include <iostream>
#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>

// Define constants
const int BURGER_COOK_TIME = 10; // Time to cook a patty in minutes
const int BURGER_TOTAL_TIME = 11; // Total time to make and package a burger in minutes

// Customer class
class Customer {
public:
    int id;
    int arrivalTime; // Time when the customer arrives
    int waitTime;    // Time the customer has to wait before receiving their order

    Customer(int id, int arrivalTime) : id(id), arrivalTime(arrivalTime), waitTime(0) {}
};

// Counter class to manage customer queues
class Counter {
public:
    std::queue<Customer*> line;

    void addCustomer(Customer* customer) {
        line.push(customer);
    }

    Customer* serveCustomer() {
        if (line.empty()) return nullptr;
        Customer* customer = line.front();
        line.pop();
        return customer;
    }

    int getLineSize() const {
        return line.size();
    }
};

// Griddle class to manage burger cooking process
class Griddle {
public:
    int capacity;
    int availableSpots;
    std::priority_queue<int, std::vector<int>, std::greater<int>> cookTimes; // Min-heap to track when a spot frees up

    Griddle(int capacity) : capacity(capacity), availableSpots(capacity) {}

    void addPatty(int finishTime) {
        cookTimes.push(finishTime);
        availableSpots--;
    }

    void releaseSpot() {
        if (!cookTimes.empty()) {
            cookTimes.pop();
            availableSpots++;
        }
    }

    int nextAvailableTime() const {
        if (cookTimes.empty()) return 0;
        return cookTimes.top();
    }

    bool isFull() const {
        return availableSpots == 0;
    }
};

// Restaurant class to manage counters and griddle
class Restaurant {
public:
    int numCounters;
    std::vector<Counter> counters;
    Griddle griddle;
    std::mutex mtx;
    std::condition_variable cv;
    int currentTime;

    Restaurant(int numCounters, int griddleCapacity) : numCounters(numCounters), counters(numCounters), griddle(griddleCapacity), currentTime(0) {}

    // Function to find the counter with the shortest line
    int findBestCounter() {
        int bestIndex = 0;
        for (int i = 1; i < numCounters; ++i) {
            if (counters[i].getLineSize() < counters[bestIndex].getLineSize()) {
                bestIndex = i;
            }
        }
        return bestIndex;
    }

    // Function to process customer orders
    void processOrders() {
        std::unique_lock<std::mutex> lock(mtx);

        for (auto& counter : counters) {
            if (!counter.line.empty()) {
                Customer* customer = counter.serveCustomer();

                // Wait for griddle to have space
                cv.wait(lock, [this]() { return !griddle.isFull(); });

                // Calculate the time the patty will be done
                int finishTime = std::max(griddle.nextAvailableTime(), customer->arrivalTime) + BURGER_COOK_TIME;

                // Add patty to the griddle
                griddle.addPatty(finishTime);

                // Set customer's wait time
                customer->waitTime = finishTime + 1 - customer->arrivalTime;

                std::cout << "Customer " << customer->id << " is served. Wait time: " << customer->waitTime << " minutes.\n";
            }
        }
    }

    // Function to simulate the griddle cooking process
    void cook() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::minutes(1));
            std::unique_lock<std::mutex> lock(mtx);

            if (griddle.nextAvailableTime() <= currentTime) {
                griddle.releaseSpot();
                cv.notify_all();
            }

            currentTime++;
        }
    }
};

int main() {
    int numCounters = 3;
    int griddleCapacity = 4;
    int numCustomers = 10;

    Restaurant restaurant(numCounters, griddleCapacity);

    // Create threads for the griddle and customer processing
    std::thread cookThread(&Restaurant::cook, &restaurant);

    std::vector<Customer*> customers;
    for (int i = 0; i < numCustomers; ++i) {
        customers.push_back(new Customer(i + 1, i * 2)); // Assuming customers arrive every 2 minutes
    }

    for (Customer* customer : customers) {
        int bestCounter = restaurant.findBestCounter();
        restaurant.counters[bestCounter].addCustomer(customer);
    }

    // Process customer orders
    std::thread processOrdersThread(&Restaurant::processOrders, &restaurant);

    processOrdersThread.join();
    cookThread.detach();

    // Calculate average waiting time
    int totalWaitTime = 0;
    for (Customer* customer : customers) {
        totalWaitTime += customer->waitTime;
    }
    double averageWaitTime = static_cast<double>(totalWaitTime) / numCustomers;

    std::cout << "Average waiting time: " << averageWaitTime << " minutes.\n";

    // Cleanup
    for (Customer* customer : customers) {
        delete customer;
    }

    return 0;
}
