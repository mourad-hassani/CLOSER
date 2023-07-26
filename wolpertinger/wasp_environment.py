from gym import Env
from gym.spaces import Box, Discrete, MultiDiscrete
import socket
import numpy as np

CACHE_SIZE = 100
NB_REQUESTS = 20

class WaspEnv(Env):
    def __init__(self):
        self.last_action = 0
        self.requests = [0 for _ in range(NB_REQUESTS)]
        self.clauses = [0 for i in range(CACHE_SIZE+NB_REQUESTS)]
        self.nb_added = 0
        self.first_step = False
        self.done = False
        self.reward = 0
        self.HOST = 'localhost'
        self.PORT = 1234

        self.s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

        self.s.connect((self.HOST, self.PORT))

        high = np.array([np.finfo(np.float32).max for i in range(3)])

        self.observation_space = Box(-high, high)
        self.action_space = Discrete(100)
        self.state_tmpr = np.array([0 for i in range(CACHE_SIZE)])
        self.state = np.array([0 for i in range(3)])

    def reset(self):
        self.last_action = 0
        self.requests = [0 for _ in range(NB_REQUESTS)]
        self.clauses = [0 for i in range(CACHE_SIZE+NB_REQUESTS)]
        self.nb_added = 0
        self.done = False
        self.reward = 0
        self.state_tmpr = np.array([0 for i in range(CACHE_SIZE)])
        self.state = np.array([0 for i in range(3)])
        if not self.done:
            self.receiveInitialize()
        return self.state

    def step(self, action):
        self.last_action = action
        print("clauses: " + str(self.clauses))
        if not self.done:
            self.receiveActions(action)
        if not self.done:
            self.receiveReward()
        if not self.done:
            self.receiveRequest()
        info = {}
        non_zeros = np.nonzero(self.state_tmpr)
        if (len(list(non_zeros[0])) > 0):
            self.state[0] = np.min(self.state_tmpr[non_zeros])
            self.state[1] = np.max(self.state_tmpr[non_zeros])
            self.state[2] = np.mean(self.state_tmpr[non_zeros])
        return np.copy(self.state), self.reward, self.done, info
    
    def receiveInitialize(self):
        terminate = False
        while not terminate:
            data = self.s.recv(1024)
            # print('Received from C++ program: ', data.decode())
            message = data.decode()
            if message.split(',')[0] == 'Initialize':
                self.first_step = True
                self.s.sendall("Initialize OK".encode())
            elif message.split(',')[0] == 'Start delete':
                self.s.sendall("Start delete OK".encode())
            elif message.split(',')[0] == 'onDeletingClause':
                if int(message.split(',')[1]) in self.clauses:
                    idx = self.clauses.index(int(message.split(',')[1]))
                    self.clauses[idx] = 0
                    self.state_tmpr[idx] = 0
                    self.s.sendall("onDeletingClause OK".encode())
                else:
                    self.s.sendall("onDeletingClause Clause not in cache".encode())
            elif message.split(',')[0] == 'onLearningClause':
                if int(message.split(',')[2]) > self.last_action:
                    self.s.sendall(str(self.nb_added).encode())
                    self.state_tmpr[self.nb_added] = int(message.split(',')[2])
                    self.clauses[self.nb_added] = int(message.split(',')[1])
                    self.nb_added = (self.nb_added + 1)%CACHE_SIZE
                else:
                    self.s.sendall(str(-1).encode())
            elif message.split(',')[0] == 'Start':
                terminate = True
                self.s.sendall("Start OK".encode())
            elif message.split(',')[0] == 'Terminate':
                terminate = True
                self.done = True
                self.s.sendall("Terminate OK".encode())
                self.s.close()
    
    def receiveActions(self, action):
        print(action)
        terminate = False
        while not terminate:
            data = self.s.recv(1024)
            # print('Received from C++ program: ', data.decode())
            message = data.decode()
            if message.split(',')[0] == 'getAction':   
                self.s.sendall(str(action[0]).encode())
                for i in range(len(list(self.state_tmpr))):
                    if (self.state_tmpr[i] >= action[0]+5 or self.state_tmpr[i] <= action[0]):
                        self.clauses[i] = 0
                        self.state_tmpr[i] = 0
            elif message.split(',')[0] == 'Start delete':
                self.s.sendall("Start delete OK".encode())
            elif message.split(',')[0] == 'onDeletingClause':
                if int(message.split(',')[1]) in self.clauses:
                    idx = self.clauses.index(int(message.split(',')[1]))
                    self.clauses[idx] = 0
                    self.state_tmpr[idx] = 0
                    self.s.sendall("onDeletingClause OK".encode())
            elif message.split(',')[0] == 'onLearningClause':
                if int(message.split(',')[2]) > self.last_action:
                    self.s.sendall(str(self.nb_added).encode())
                    self.state_tmpr[self.nb_added] = int(message.split(',')[2])
                    self.clauses[self.nb_added] = int(message.split(',')[1])
                    self.nb_added = (self.nb_added + 1)%CACHE_SIZE
                else:
                    self.s.sendall(str(-1).encode())
            elif message.split(',')[0] == 'Start Reward':
                self.s.sendall("Start Reward OK".encode())
                terminate = True
            elif message.split(',')[0] == 'Terminate':
                terminate = True
                self.done = True
                self.s.sendall("Terminate OK".encode())
                self.s.close()

    def receiveReward(self):
        terminate = False
        while not terminate:
            data = self.s.recv(1024)
            # print('Received from C++ program: ', data.decode())
            message = data.decode()
            if message.split(',')[0] == 'reward':
                self.reward = int(message.split(',')[1])
                print('Reward: ' + str(self.reward))
                self.s.sendall('reward OK'.encode())
            elif message.split(',')[0] == 'Start Request':
                terminate = True
                self.s.sendall("Start Request OK".encode())
            elif message.split(',')[0] == 'Terminate':
                terminate = True
                self.done = True
                self.s.sendall("Terminate OK".encode())
                self.s.close()

    def receiveRequest(self):
        for i in range(NB_REQUESTS):
            self.clauses[CACHE_SIZE+i] = 0
            self.requests[i] = 0
        request_index = 0
        terminate = False
        while not terminate:
            data = self.s.recv(1024)
            # print('Received from C++ program: ', data.decode())
            message = data.decode()
            if message.split(',')[0] == 'requested':
                self.clauses[CACHE_SIZE+request_index] = int(message.split(',')[1])
                self.requests[request_index] = int(message.split(',')[2])
                request_index += 1
                print('requested: ' + str(message.split(',')[1]))
                self.s.sendall('requested OK'.encode())
            elif message.split(',')[0] == 'Start':
                terminate = True
                self.s.sendall("Start OK".encode())
            elif message.split(',')[0] == 'Terminate':
                terminate = True
                self.done = True
                self.s.sendall("Terminate OK".encode())
                self.s.close()