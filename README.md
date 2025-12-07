# Restaurant Order Management System (Queue Based)

This project is a backend restaurant order management system built using C++ and the Crow web framework. It demonstrates the practical use of the Queue data structure by simulating how real-world restaurant orders are handled, prioritized, prepared, and completed.

The system runs entirely in memory and exposes all functionality through REST APIs, making it easy to connect with any frontend.

# Project Overview

Restaurants handle multiple types of orders at the same time such as:

Dine-in

Takeout

Delivery

Each order type has a different urgency level. This project uses separate queues for each order type and processes them based on a simple priority system:

Delivery (highest priority)

Takeout

Dine-in (lowest priority)

Once an order is picked, it moves into a preparing queue, and after completion, it moves into a completed queue.

# Key Features

Place new restaurant orders

Automatic priority selection based on order type

Fetch the next order to prepare

Mark orders as completed

View the current state of all queues

View individual queues separately

Clear completed orders

Fully memory-based (no database used)
