#! "C:\Python3.3\python.exe"
from socket import *
import sys
import time

####
# set server contact info
##
#serverName= 'X.Y.Z.W'
#serverName= '140.232.234.159'
#serverName= '10.202.63.151'
#serverName= '192.168.1.186'
serverName = '192.168.68.111'
#serverName = '10.255.224.71'

serverPort= 80
validPort = 200

####
# set my contact info
##
myIncomingPort= 1025

####
# reach out to sensor to send registration
##
clientSocket= socket(AF_INET, SOCK_STREAM)

clientSocket.connect((serverName, serverPort))

#request= "register X.Y.Z.W,myport,rising,100\n"
request= "register 192.168.68.108,1025,,scanning,20000,100,9999999999999999\n"
#request= "register 140.232.233.57,1025,temperature,any,150000,100,40000\n"

print("registration client sending request: ")
print(request + "\n")

clientSocket.send(request.encode())

####
# obtain verification of registration
##
response= clientSocket.recv(1024)
decodedResponse= response.decode()

print(f'received response from registration server: {decodedResponse}\n')

clientSocket.close()
print('closed registration client\n')

####
# create a server to receive the pushed data
##
serverSocket = socket(AF_INET, SOCK_STREAM)
serverSocket.bind(('',myIncomingPort))
serverSocket.listen(1)

print('Ready to receive data...')

try:
    while True:      
      connectionSocket, addr= serverSocket.accept()
      incomingMsg= connectionSocket.recv(1024)
      incomingMsg = incomingMsg.decode()
      print(incomingMsg)
      connectionSocket.close()
      time.sleep(1)
      validationSocket = socket(AF_INET, SOCK_STREAM)
      validationSocket.connect((serverName, serverPort))

      if(incomingMsg == "209:169:85:6"):
            validationSocket.send(("VALID\n").encode())
            print("Valid RFID")
      else:
            validationSocket.send(("NOT_VALID\n").encode())
            print("Non-Valid RFID")

      validationSocket.close()
except KeyboardInterrupt:
      print("\nCtrl+C detected")
      connectionSocket.close()
      validationSocket.close()
      sys.exit(0)