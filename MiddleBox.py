import csv
from datetime import datetime
from socket import *
import sys
import threading
import time

DEBUG = False

####
# set server contact info
##
serverName = '192.168.1.78'
serverPort = 80


####
# set my contact info
##
myIncomingPort= 1025

####
# variables that hold the contact information
# for the Arduino process registration server
##
clientSocket= None
serverSocket= None
webSocket = None

#####
# Example registration protocol messages
#
# request= "register X.Y.Z.W,myport,rising,100\n"
# request= "register 192.168.1.173,1025,humidity,any,20000,100,40000\n"
# request= "register 140.232.234.86,1025,temperature,any,345000,1000,20000\n"
#####


#####
# number of threads for the responder server that pick off incoming client connections
# and service their requests.
###
numReqHandlerThreads= 5

#####
# registrations located in file
##
registrationFile= "registrations.csv"

#####
# Client requests and its lock
##
clientRequests= []
clientRequestLock= threading.Lock()

#####
# DataBase and its lock
##
databaseName= "Students.csv"
databaseLock= threading.Lock()

databaseEntryLock= threading.Lock()
databaseEntries= []

webClients = []
webClientsLock = threading.Lock()

#####
# Client request handler class
#
# Server side semantics for responder server interactions with responder
# client for data push
###
class ClientRequestHandler:
    def __init__(self,connectionSock):
        self.connectionSock= connectionSock
        self.decodedMsg = self.connectionSock.recv(1024).decode()

    def close_connection(self):
        self.connectionSock.close()

    def get_message(self):
        return self.decodedMsg
    
    def send_validation(self, isValid):
        time.sleep(1)
        validationSocket = socket(AF_INET, SOCK_STREAM)
        validationSocket.connect((serverName, serverPort))

        if(isValid):
                validationSocket.send(("VALID\n").encode())
                print("Valid RFID")
        else:
                validationSocket.send(("NOT_VALID\n").encode())
                print("Non-Valid RFID")

        validationSocket.close()

####
# Container holding the ID of the desired change and
# a boolean representing the increase or decrease in
# the amount of food that ID will have
####
class DatabaseEntry:
    def __init__(self, id, orderedFood):
        self.id = id
        self.orderedFood = orderedFood

    def isSameStudent(self, id):
        return self.id == id
    
    def getNewValue(self, currentValue):
        if(self.orderedFood):
            return currentValue + 1
        else:
            if(currentValue <= 0):
                return 0
            
            return currentValue - 1

def create_web_server():
    serverSock = socket(AF_INET, SOCK_STREAM)
    serverSock.bind(("0.0.0.0", 80))
    serverSock.listen(5)
    print('create_web_server:: Ready to receive requests...')
    return serverSock

def web_server():
    global webSocket
    webSocket = create_web_server()
    t = threading.Thread(target=process_web_clients, daemon=True)
    t.start()
    web_server_req_handler(webSocket)

####
# Receives web server requests
# If it is a POST request, try to add its contents to the databaseEntries list
# Append webclients list with client socket to return web page to them
####
def web_server_req_handler(serverSock):
    while True:
        (clientSocket, address) = serverSock.accept()
        rd = clientSocket.recv(5000).decode()
        pieces = rd.split("\n")
        
        if(len(pieces) == 0 or pieces[0].__len__ == 0):
            continue

        split = str.split(pieces[0])

        if(split.__len__() > 0 and str.split(pieces[0])[0] == "POST"):
            start = -1

            for i in range(len(pieces)):
                if(DEBUG):
                    print(pieces[i])
                
                if(pieces[i] == "\r"):
                    start = i + 1
                    break

            if(DEBUG):
                print(start)

            if(start >= 0 and start < len(pieces)): 

                id = pieces[start].split('=')[1]

                try:
                    idNum = int(id)
                    entry = DatabaseEntry(idNum, True)

                    databaseEntryLock.acquire()

                    databaseEntries.append(entry)

                    databaseEntryLock.release()
                except:
                    pass

                if(DEBUG):
                    print("Adding DB Entry for " + id)

        elif(DEBUG):
            print("GET")


        webClientsLock.acquire()

        webClients.append(clientSocket)

        webClientsLock.release()

        if(DEBUG):
            print("Added connection")

####
# If there is one or more web clients currently, return a webpage with all current orders listed
####
def process_web_clients():
    global databaseLock
    global webClients
    global webClientsLock

    while(True):
        
        webClientsLock.acquire()
        
        count = webClients.__len__()

        webClientsLock.release()

        if(count == 0):
            continue

        print("Processing...")

        data = "HTTP/1.1 200 OK\r\n"
        data += "Content-Type: text/html; charset=utf-8\r\n"
        data += "\r\n"
        data += '<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0"><title>Document</title></head><body><h1>Order Submission</h1><form method="post"><label for="id">ID </label><input type="number" id="id" name="id"></input><br><br><label for="food">Food </label><select id="food"><option value="Burger">Burger</option><option value="Pizza">Pizza</option><option value="Chicken Patty">Chicken Patty</option></select><br><br><button>Submit</button></form><br><h1>Current Orders</h1>'

        databaseLock.acquire()

        with open(databaseName, mode='r', newline='', encoding='utf-8') as csvfile:
            csv_reader = csv.reader(csvfile, delimiter=',')
            
            for row in csv_reader:
                if(row.__len__() < 4):
                    continue
                    
                if(int(row[3]) > 0):
                    data += "<div><h2>Name: "
                    data += row[1]
                    data += "</h3><h3>ID: "
                    data += row[0]
                    data += "</h2><h3>Orders: "
                    data += row[3]
                    data += "</h2></div>"

        databaseLock.release()

        data += '<style>body{background-color: indianred;}body label{font-size: 1.75em;color: black;font-weight: bold;}body input{font-size: 1.5em;border-width: 2px;border-color: black;background-color: yellow;}body select{background-color: rgb(0, 255, 38);font-size: 1.5em;border-width: 2px;border-color: black;}body button{background-color: blue;color: white;font-size: 1.5em;border-radius: 10px;border-width: 2px;border-color: black;}body div{background-color: white;width: 300px;border-width: 3px;border-color: black;border-radius: 10px;padding-left: 10px;border-style: solid;margin-top: 10px;}</style></body></html>'

        webClientsLock.acquire()
        
        for client in webClients:
            client.sendall(data.encode())
            client.shutdown(SHUT_WR)
            client.close()

        webClients.clear()

        webClientsLock.release()

        print("Sent response")

####
# If there is a database entry in queue, rewrite database with the desired edit
####
def database_writer():
    global databaseLock
    global databaseEntries
    global databaseEntryLock

    while True:
        ## Thread safe get db entry
        databaseEntryLock.acquire()

        numDBEntries = len(databaseEntries)

        if numDBEntries > 0:
            dbEntry= databaseEntries.pop(0)
        else:
            dbEntry= None

        databaseEntryLock.release()

        if(dbEntry == None):
            continue

        arr = []

        databaseLock.acquire()

        with open(databaseName, mode = "r") as file:
            reader = csv.reader(file, delimiter=',')
            index = 0

            for row in reader:
                if(row.__len__() < 3):
                    continue

                arr.append(row)

                if(dbEntry.isSameStudent(int(row[0]))):
                    arr[index][3] = dbEntry.getNewValue(int(row[3]))
                
                index += 1

        with open(databaseName, "w+") as file:
            writer = csv.writer(file)

            if(DEBUG):
                print(arr)

            for i in range(len(arr)):
                writer.writerow(arr[i])

            file.flush()

        databaseLock.release()

########
# create_responder_server
#
# create a server to receive the pushed data
###

def create_responder_server():
    serverSock = socket(AF_INET, SOCK_STREAM)
    serverSock.bind(('',myIncomingPort))
    serverSock.listen(1)
    print('create_responder_server:: Ready to receive data...')
    return serverSock


########
# responder_request_handler
#
#  accept incoming connection, create handler object,
#  and append it to set of client connections
##

def responder_server_req_handler(serverSock):
    while True:
        connectionSocket, addr= serverSock.accept()
        clientRequest = ClientRequestHandler(connectionSocket)
        print("Scanner got " + clientRequest.get_message())

        clientRequestLock.acquire()
        clientRequests.append(clientRequest)
        clientRequestLock.release()


def responder_server():
    global serverSocket
    serverSocket= create_responder_server()
    t= threading.Thread(target=request_handler, daemon=True)
    t.start()
    responder_server_req_handler(serverSocket)

########
# request_handler
#
# responder server
# handles interaction with the responder client by receiving
# sensor measurement, constructing database entry, and appending
# it to list of entries
###
def request_handler():
    global clientRequestLock
    global databaseEntryLock
    global databaseEntries

    while True:

        ## Thread safe get client request
        clientRequestLock.acquire()

        if len(clientRequests) > 0:
            clientRequest= clientRequests.pop(0)
        else:
            clientRequest= None

        clientRequestLock.release()

        ###
        # If you have a responder client request, then
        # get it from the client and append the delivered
        # measurement to the list of Db entries to be written
        ##
        if (clientRequest != None):

            # clientRequest.get_request()
            theMessage = clientRequest.get_message()

            id = int(valid_RFID(theMessage))
            isValid = id >= 0

            clientRequest.send_validation(isValid)
            clientRequest.close_connection()

            if(isValid == False):
                continue

            newDBEntry = DatabaseEntry(id, False)

            ## thread safe add newly delivered data
            ## to the list of entries to be written to database
            databaseEntryLock.acquire()

            databaseEntries.append(newDBEntry)

            databaseEntryLock.release()

        else:
            time.sleep(0.5)

####
# Returns ID of valid RFID
# If it is not valid, it will return a negative number
####
def valid_RFID(rfid):
    res = -1
    
    databaseLock.acquire()

    with open(databaseName, mode = "r") as file:
        reader = csv.reader(file, delimiter=',')

        for row in reader:
            if(row.__len__() < 3):
                continue

            if(rfid == row[2]):
                if(int(row[3]) > 0):
                    res = row[0]

                break

    databaseLock.release()

    return res              


def main():
    # Start database writer thread
    writerThread= threading.Thread(target=database_writer, daemon=True)
    writerThread.start()
    
    #####
    # start the web server
    ###
    webThread = threading.Thread(target=web_server, daemon=True)
    webThread.start()

    #####
    # start the responder server
    ###
    serverThread= threading.Thread(target=responder_server,daemon= True)
    serverThread.start()

    clientSocket = socket(AF_INET, SOCK_STREAM)
    clientSocket.connect((serverName, serverPort))
    request = "register 192.168.1.24,1025,,scanning,20000,100,9999999999999999\n"
    clientSocket.send(request.encode())
    clientSocket.close()

    while(True):
        time.sleep(1)

main()