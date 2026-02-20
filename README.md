# Bistro Locker 🍔🔒
This was the final project for my Internet of Things course in Fall 2025. The project is broken into two major components:  
* 🔌 Arduino Process
* 🛢 Python Middlebox Server

## 🌟 Highlights

* Designed a scalable FSM architecture
* Implemented TCP client-server communication on an Arduino microcontroller
* Integrated SPI-based RFID hardware with network authentication
* Managed asynchronous validation with timeout safeguards
* Structured parsing using tokenized command messages
* Designed a distributed system integrating embedded IoT hardware with backend services
* Built a thread-safe producer–consumer database update pipeline
* Implemented socket-level HTTP parsing without external frameworks
* Developed a real-time authentication protocol over TCP
* Applied synchronization techniques to prevent race conditions

## ℹ️ Overview
This system was prototyped to address a problem in regards to one of my university's dining options: The Bistro.  

### Problem
The Bistro has an order system that is very prone to thievery. When your food is complete, it is put on a shelf for you to grab at your own time. However, anyone can walk up and grab your food at any time and steal it. This means that students don't get their food, workers have to remake orders on a already demanding backlog, and administration has to spend extra to remake the food.

### Inspiration
In order to solve this, I looked to what we currently had available to us. Our school uses an ID scanning system, where the RFID inside of the ID chip is queried to the university server to check permissions. Looking outside of the university, I remembered how amusement and water parks that have locker systems based on pin. By combining these two concepts, we could implement a dynamic safeguard for ordered food.

### Solution
The idea was to have a RFID scanner process that will send ID info to a main server. This server will analyze the database of current orders, attempting to find the same ID sent from the scanner. If there is no equivalent ID, the server will send back a rejection signal. If there is an order of that nature, send an acceptance signal which will open the locker door, revealing the client's food.

## 🔌 Arduino Process

WiFi-Connected RFID Smart Lock System (Arduino)

This project is a network-enabled RFID smart lock system built on Arduino that integrates wireless communication, event-driven logic, and hardware control using a Finite State Machine (FSM) architecture. The system scans RFID cards, validates credentials through a remote server, and controls a servo-actuated locking mechanism.

### ⚙️ Key Features
* Robust WiFi connection handling with retry logic
* Token-based message parsing
* Time-window-based event firing
* Modular, readable state-machine implementation

### 🔧 Technologies & Hardware Used

* Arduino (WiFi-enabled board using WiFiS3 library)
* PN532 RFID module (via SPI)
* Servo motor (door lock mechanism)
* Push button (door close detection)
* WiFi TCP client/server communication
* C++ (Arduino framework)

### 🧠 System Architecture

The project is designed around a Finite State Machine (FSM) for clean state transitions and modular logic separation. The primary states include:
* GET_REQUEST – Waits for registration from a remote server
* PARSE_REQUEST – Parses and validates incoming configuration
* SENSE_RFID – Scans for RFID credentials
* SEND_RESPONSE – Sends scanned UID to validation server
* VALIDATE – Waits for access approval
* UNLOCK – Actuates servo to unlock
* SENSE_CLOSE – Waits for user button press
* LOCK – Re-locks the system

This design ensures non-blocking behavior, predictable execution flow, and scalable expansion.

### 🌐 Network Registration & Event Configuration

The Arduino acts as a lightweight server and registers with the Python middlebox. It parses structured registration messages containing:
* Target IP and port
* Event type
* Start time
* Validation interval
* Active duration

This allows the system to operate only within defined time windows and control how frequently RFID events are transmitted.

### 📡 RFID Scanning & Validation Flow

The PN532 module scans for ISO14443A RFID cards. When detected, the UID is formatted and transmitted to a remote server. The Arduino waits for a "VALID" or "NOT_VALID" response. A timeout mechanism ensures the system doesn’t stall if no response is received.

Upon validation:
* "VALID" → Servo unlocks door
* "NOT_VALID" → System returns to scanning mode

This creates a secure client-server authentication workflow.

### 🔒 Physical Access Control

Servo Motor simulates a locking mechanism:
* 180° → Unlock
* 0° → Lock

Push Button detects when the door is closed before re-locking.
Time-based validation ensures secure session handling

## 🛢 Python Middlebox Server

Multi-Threaded RFID Validation & Web Management Server

### ⚙️ Key Features

* Multi-threaded TCP server architecture
* Custom HTTP server built from raw sockets
* Real-time RFID validation
* CSV-based persistent storage
* Asynchronous database update queue
* Lock-based concurrency control
* Event-driven responder design

### 🧠 System Overview

The application runs three concurrent services:
* Responder Server – Receives RFID scans from the Arduino and validates credentials
* Web Server – Hosts a live order management interface
* Database Writer Thread – Safely updates CSV-based records

Threading and synchronization primitives (Lock) ensure safe concurrent access to shared resources.

### 📡 RFID Validation Workflow
Arduino sends scanned RFID UID via TCP
Server checks UID against Students.csv
Validation rules:
* UID must exist
* Student must have available “orders” (balance > 0)

Server responds with: VALID or NOT_VALID

If VALID:
* Decrements student order count
* Updates database asynchronously

This creates a full client-server authentication loop between embedded hardware and backend logic.

### 🌐 Built-In Web Server
The system includes a custom socket-based HTTP server that:
* Serves an HTML order submission form
* Accepts POST requests
* Displays current active orders
* Dynamically renders student order counts from CSV

This allows users to order digitally through a web interface.

### 🗄 Database Management

Using a CSV file as a persistent storage, this process:

* Implements thread-safe read/write operations
* Processes updates through a queue (databaseEntries)
* Applies modifications via a dedicated writer thread

This design prevents race conditions while maintaining responsiveness.

### 🔒 Concurrency & Thread Safety

The system demonstrates careful multi-threaded design:
* clientRequests queue with lock protection
* databaseEntries queue with lock protection

Dedicated locks for:
* Database file access
* Entry modification
* Web client connections

This prevents data corruption and ensures consistent state across services.

## ✍️ Author
[Nicholas Lyons](https://github.com/Heirioten)
