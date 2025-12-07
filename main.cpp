#include <iostream>
#include <string>
#include <mutex>
#include <ctime>
#include <queue>
#define CROW_USE_BOOST
#define CROW_MAIN

#include <boost/asio.hpp>
namespace asio = boost::asio;
#include "crow.h"

using namespace std;


struct OrderNode {
    int id;
    string customerName;
    string orderType;
    string items;
    string status;
    string timestamp;
    int priority;
    OrderNode* next;

    OrderNode(int id, const string& name, const string& type, const string& items,
              const string& timestamp, int priority)
        : id(id), customerName(name), orderType(type), items(items),
          status("pending"), timestamp(timestamp), priority(priority), next(nullptr) {}
};


class OrderQueue {
private:
    OrderNode* front;
    OrderNode* rear;
    int count;
    string queueType;

public:
    OrderQueue(const string& type) : front(nullptr), rear(nullptr), count(0), queueType(type) {}

    void enqueue(OrderNode* order) {
        if (rear == nullptr) {
            front = rear = order;
        } else {
            rear->next = order;
            rear = order;
        }
        count++;
    }

    OrderNode* dequeue() {
        if (front == nullptr) return nullptr;

        OrderNode* temp = front;
        front = front->next;

        if (front == nullptr) {
            rear = nullptr;
        }

        count--;
        temp->next = nullptr;
        return temp;
    }

    OrderNode* peek() {
        return front;
    }

    bool isEmpty() {
        return front == nullptr;
    }

    int getCount() {
        return count;
    }

    crow::json::wvalue toJSON() {
        crow::json::wvalue result;
        result["queueType"] = queueType;
        result["count"] = count;
        result["orders"] = crow::json::wvalue::list();

        OrderNode* current = front;
        int index = 0;

        while (current != nullptr) {
            crow::json::wvalue order;
            order["id"] = current->id;
            order["customerName"] = current->customerName;
            order["orderType"] = current->orderType;
            order["items"] = current->items;
            order["status"] = current->status;
            order["timestamp"] = current->timestamp;
            order["priority"] = current->priority;

            result["orders"][index++] = move(order);
            current = current->next;
        }

        return result;
    }

    ~OrderQueue() {
        while (front != nullptr) {
            OrderNode* temp = front;
            front = front->next;
            delete temp;
        }
    }
};


class OrderManager {
private:
    OrderQueue dineInQueue;
    OrderQueue takeoutQueue;
    OrderQueue deliveryQueue;
    OrderQueue preparingQueue;
    OrderQueue completedQueue;

    int orderIdCounter;
    mutex mtx;

    string getCurrentTimestamp() {
        time_t now = time(0);
        char* dt = ctime(&now);
        string timestamp(dt);
        if (!timestamp.empty() && timestamp.back() == '\n') {
            timestamp.pop_back();
        }
        return timestamp;
    }

    int getPriority(const string& orderType) {
        if (orderType == "delivery") return 1;
        if (orderType == "takeout") return 2;
        return 3; // dine-in
    }

    OrderQueue& getQueueByType(const string& orderType) {
        if (orderType == "delivery") return deliveryQueue;
        if (orderType == "takeout") return takeoutQueue;
        return dineInQueue;
    }

public:
    OrderManager()
        : dineInQueue("dine-in"), takeoutQueue("takeout"), deliveryQueue("delivery"),
          preparingQueue("preparing"), completedQueue("completed"), orderIdCounter(0) {}

    crow::json::wvalue placeOrder(const string& customerName, const string& orderType, const string& items) {
        lock_guard<mutex> lock(mtx);

        int priority = getPriority(orderType);
        OrderNode* newOrder = new OrderNode(
            ++orderIdCounter, customerName, orderType, items, getCurrentTimestamp(), priority
        );

        OrderQueue& targetQueue = getQueueByType(orderType);
        targetQueue.enqueue(newOrder);

        crow::json::wvalue response;
        response["status"] = "success";
        response["orderId"] = newOrder->id;
        response["customerName"] = customerName;
        response["orderType"] = orderType;
        response["message"] = "Order placed successfully";
        response["queuePosition"] = targetQueue.getCount();

        return response;
    }

    crow::json::wvalue getNextOrder() {
        lock_guard<mutex> lock(mtx);

        // Priority: delivery > takeout > dine-in
        OrderNode* order = nullptr;

        if (!deliveryQueue.isEmpty()) {
            order = deliveryQueue.dequeue();
        } else if (!takeoutQueue.isEmpty()) {
            order = takeoutQueue.dequeue();
        } else if (!dineInQueue.isEmpty()) {
            order = dineInQueue.dequeue();
        }

        crow::json::wvalue response;
        if (order) {
            order->status = "preparing";
            preparingQueue.enqueue(order);

            response["status"] = "success";
            response["order"]["id"] = order->id;
            response["order"]["customerName"] = order->customerName;
            response["order"]["orderType"] = order->orderType;
            response["order"]["items"] = order->items;
            response["order"]["status"] = order->status;
        } else {
            response["status"] = "empty";
            response["message"] = "No pending orders";
        }

        return response;
    }

    crow::json::wvalue completeOrder(int orderId) {
        lock_guard<mutex> lock(mtx);

        // Find order in preparing queue
        OrderNode* prev = nullptr;
        OrderNode* current = preparingQueue.peek();

        while (current != nullptr && current->id != orderId) {
            prev = current;
            current = current->next;
        }

        crow::json::wvalue response;
        if (current == nullptr) {
            response["status"] = "error";
            response["message"] = "Order not found in preparing queue";
            return response;
        }

        // Remove from preparing queue
        if (prev == nullptr) {
            preparingQueue.dequeue();
        } else {
            prev->next = current->next;
        }

        current->status = "completed";
        current->next = nullptr;
        completedQueue.enqueue(current);

        response["status"] = "success";
        response["orderId"] = orderId;
        response["message"] = "Order completed";

        return response;
    }

    crow::json::wvalue getAllQueues() {
        lock_guard<mutex> lock(mtx);

        crow::json::wvalue response;
        response["dineIn"] = dineInQueue.toJSON();
        response["takeout"] = takeoutQueue.toJSON();
        response["delivery"] = deliveryQueue.toJSON();
        response["preparing"] = preparingQueue.toJSON();
        response["completed"] = completedQueue.toJSON();

        response["stats"]["totalPending"] =
            dineInQueue.getCount() + takeoutQueue.getCount() + deliveryQueue.getCount();
        response["stats"]["totalPreparing"] = preparingQueue.getCount();
        response["stats"]["totalCompleted"] = completedQueue.getCount();

        return response;
    }

    crow::json::wvalue getQueueStatus(const string& queueType) {
        lock_guard<mutex> lock(mtx);

        if (queueType == "dine-in") return dineInQueue.toJSON();
        if (queueType == "takeout") return takeoutQueue.toJSON();
        if (queueType == "delivery") return deliveryQueue.toJSON();
        if (queueType == "preparing") return preparingQueue.toJSON();
        if (queueType == "completed") return completedQueue.toJSON();

        crow::json::wvalue error;
        error["error"] = "Invalid queue type";
        return error;
    }

    void clearCompleted() {
        lock_guard<mutex> lock(mtx);

        while (!completedQueue.isEmpty()) {
            OrderNode* order = completedQueue.dequeue();
            delete order;
        }
    }
};


int main() {
    crow::SimpleApp app;
    OrderManager orderManager;

    // Root endpoint
    CROW_ROUTE(app, "/")([]() {
        crow::response res("Restaurant Order Management System - Queue Implementation\n"
                          "Endpoints:\n"
                          "  POST /api/orders - Place new order\n"
                          "  GET /api/orders/next - Get next order to prepare\n"
                          "  POST /api/orders/:id/complete - Mark order as completed\n"
                          "  GET /api/queues - Get all queues status\n"
                          "  GET /api/queues/:type - Get specific queue (dine-in/takeout/delivery/preparing/completed)\n"
                          "  DELETE /api/completed - Clear completed orders");
        res.add_header("Access-Control-Allow-Origin", "*");
        return res;
    });

    // POST - Place new order
    CROW_ROUTE(app, "/api/orders").methods("POST"_method)
    ([&orderManager](const crow::request& req) {
        auto body = crow::json::load(req.body);

        if (!body || !body.has("customerName") || !body.has("orderType") || !body.has("items")) {
            crow::response res(400, "Missing required fields: customerName, orderType, items");
            res.add_header("Access-Control-Allow-Origin", "*");
            return res;
        }

        string customerName = body["customerName"].s();
        string orderType = body["orderType"].s();
        string items = body["items"].s();

        if (orderType != "dine-in" && orderType != "takeout" && orderType != "delivery") {
            crow::response res(400, "Invalid orderType. Must be: dine-in, takeout, or delivery");
            res.add_header("Access-Control-Allow-Origin", "*");
            return res;
        }

        auto result = orderManager.placeOrder(customerName, orderType, items);
        crow::response res(201, result);
        res.add_header("Access-Control-Allow-Origin", "*");
        return res;
    });

    // GET - Get next order
    CROW_ROUTE(app, "/api/orders/next")
    ([&orderManager]() {
        auto result = orderManager.getNextOrder();
        crow::response res(result);
        res.add_header("Access-Control-Allow-Origin", "*");
        return res;
    });

    // POST - Complete order
    CROW_ROUTE(app, "/api/orders/<int>/complete").methods("POST"_method)
    ([&orderManager](int orderId) {
        auto result = orderManager.completeOrder(orderId);
        crow::response res(result);
        res.add_header("Access-Control-Allow-Origin", "*");
        return res;
    });

    // GET - All queues
    CROW_ROUTE(app, "/api/queues")
    ([&orderManager]() {
        auto result = orderManager.getAllQueues();
        crow::response res(result);
        res.add_header("Access-Control-Allow-Origin", "*");
        return res;
    });

    // GET - Specific queue
    CROW_ROUTE(app, "/api/queues/<string>")
    ([&orderManager](string queueType) {
        auto result = orderManager.getQueueStatus(queueType);
        crow::response res(result);
        res.add_header("Access-Control-Allow-Origin", "*");
        return res;
    });

    // DELETE - Clear completed
    CROW_ROUTE(app, "/api/completed").methods("DELETE"_method)
    ([&orderManager]() {
        orderManager.clearCompleted();
        crow::json::wvalue response;
        response["status"] = "success";
        response["message"] = "Completed orders cleared";

        crow::response res(response);
        res.add_header("Access-Control-Allow-Origin", "*");
        return res;
    });

    // CORS Preflight
    CROW_ROUTE(app, "/<path>").methods("OPTIONS"_method)
    ([](const crow::request&, crow::response& res, string) {
        res.add_header("Access-Control-Allow-Origin", "*");
        res.add_header("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
        res.add_header("Access-Control-Allow-Headers", "Content-Type");
        res.code = 200;
        res.end();
    });


    app.port(8080).multithreaded().run();

    return 0;
}