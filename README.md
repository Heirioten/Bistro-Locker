# Bistro Locker 🍔🔒
This was the final project for my Internet of Things course in Fall 2025. The project is broken into three major components:  
* 🔌 Arduino Process
* 🛜 Client-Server Connection
* 🛢 Concurrent Database Management

## ℹ️ Overview
This system was prototyped to address a problem in regards to one of my school's dining options: The Bistro.  

### Problem
The Bistro has an order system that is very prone to thievery. When your food is complete, it is put on a shelf for you to grab at your own time. However, anyone can walk up and grab your food at any time and steal it. This means that students don't get their food, workers have to remake orders on a already demanding backlog, and administration has to spend extra to remake the food.

### Inspiration
In order to solve this, I looked to what we currently had available to us. Our school uses an ID scanning system, where the RFID inside of the ID chip is queried to the university server to check permissions. Looking outside of the university, I remembered how amusement and water parks that have locker systems based on pin. By combining these two concepts, we could implement a dynamic safeguard for ordered food.

### Solution
The idea was to have a RFID scanner process that will send ID info to a main server. This server will analyze the database of current orders, attempting to find the same ID sent from the scanner. If there is no equivalent ID, the server will send back a rejection signal. If there is an order of that nature, send an acceptance signal which will open the locker door, revealing the client's food.

## 🔌 Arduino Process

## 🛜 Client-Server Connection

## 🛢 Concurrent Database Management

## ✍️ Author
[Nicholas Lyons](https://github.com/Heirioten)
