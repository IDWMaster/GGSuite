/*
	This file is part of the GlobalGrid Protocol Suite.

    GlobalGrid Protocol Suite is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    GlobalGrid Protocol Suite is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with GlobalGrid Protocol Suite.  If not, see <http://www.gnu.org/licenses/>.
*/



#ifndef IP_H
#define IP_H
#include <GlobalGrid.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>
#include <string>
#include <map>
#include <stdio.h>
#include <mutex>
#include <uuid/uuid.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
namespace GlobalGrid {
class IPAbstractDriver:public ProtocolDriver {
public:
	std::map<uint64_t,::VSocket*> socketMappings;
	std::recursive_mutex mtx;
};
class InternetSocket:public VSocket {
public:
	int server;
	struct sockaddr_in destination;
	IPAbstractDriver* driver;
	InternetSocket(int server, struct sockaddr_in destination, IPAbstractDriver* driver) {
		this->server = server;
		this->destination = destination;
		this->driver = driver;
	}
	void Send(SafeBuffer& buffer) const {
		//Transfer data
		sendto(server,buffer.data,buffer.len,0,(struct sockaddr*)&destination, sizeof(destination));
	}
	void Serialize(SafeBuffer& output) const {
		//TODO: Implement later
		printf("TODO: Implement serialization\n");
	}
	~InternetSocket() {
		//printf("IP DEBUG: Socket release\n");
		std::lock_guard<std::recursive_mutex>(driver->mtx);
        uint64_t addr = (uint64_t)(destination.sin_addr.s_addr);
                            uint64_t port = (uint64_t)ntohs(destination.sin_port);
							uint64_t val = 0;
							//Fill first 32-bits
							val = addr;
							//Fill second 32-bits
							val |= port<<32;
							auto sval = driver->socketMappings.find(val);
							if(sval == driver->socketMappings.end()) {
							  printf("ERROR destroying IP socket -- Fault code 0x0\n");
							  throw "sideways";
							}
		driver->socketMappings.erase(driver->socketMappings.find(val));

	}
};
class InternetProtocol:public IPAbstractDriver {
public:
	int server;
	std::shared_ptr<P2PConnectionManager> mngr;
	bool running;
	std::thread* mtr;
    std::vector<struct sockaddr_in> knownRoutes;
    std::map<uint64_t,bool> routers;
    void Broadcast(const SafeBuffer& buffy) const {
		sendto(server,buffy.data,buffy.len,0,(struct sockaddr*)&broadcastAddr,sizeof(broadcastAddr));
		sendto(server,buffy.data,buffy.len,0,(struct sockaddr*)&localAddr,sizeof(broadcastAddr));

        //TODO: Broadcast to all available routers
        for(uint32_t i = 0;i <knownRoutes.size();i++) {
            sockaddr_in route = knownRoutes[i];
            sendto(server,buffy.data,buffy.len,0,(struct sockaddr*)&route,sizeof(route));

        }

	}

	struct sockaddr_in localAddr;
	struct sockaddr_in broadcastAddr;
	VSocket* Deserialize(const SafeBuffer& buffer) {
		//TODO: Implement deserialize
		return 0;
	}
    FILE* fptr;
	InternetProtocol(int port, std::shared_ptr<P2PConnectionManager> mngr) {
        fptr = fopen("ipcache.db","r+b");
        if(fptr == 0) {
            fptr = fopen("ipcache.db","w+b");
        }else {
            uint32_t len = 0;
            fread(&len,1,4,fptr);
            size_t count = (size_t)len;

          //  printf("Found %i routes\n",(int)len);
            knownRoutes.resize(count);
            for(size_t i = 0;i<count;i++) {
                uint64_t val;
                fread(&val,1,8,fptr);
                struct sockaddr_in addr;
                memset(&addr,0,sizeof(addr));
                addr.sin_addr.s_addr = ((uint32_t)val);
                addr.sin_port = ((uint32_t)(val >> 32));
                //printf("%i\n",addr.sin_port);
                addr.sin_family = AF_INET;
                knownRoutes[i] = addr;
                //TODO: Compress into 64-bit integer
                routers[val] = true;

            }
        }
		uuid_parse("eb4acca0-37df-11e4-916c-0800200c9a66",protoID);
		server = socket(AF_INET, SOCK_DGRAM,IPPROTO_UDP);
		running = true;
		//Try to bind
		struct sockaddr_in serverAddr;

		socklen_t mlen = sizeof(serverAddr);
		memset(&serverAddr,0,sizeof(serverAddr));
		serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_port = htons(port);
		this->mngr = mngr;
		struct ip_mreq multicast;
		memset(&multicast,0,sizeof(multicast));
		multicast.imr_multiaddr.s_addr = inet_addr("239.0.0.1");
		multicast.imr_interface.s_addr = htonl(INADDR_ANY);
		memset(&broadcastAddr,0,sizeof(broadcastAddr));
		broadcastAddr.sin_addr.s_addr = inet_addr("239.0.0.1");
		broadcastAddr.sin_port = htons(port);
		broadcastAddr.sin_family = AF_INET;
		memset(&localAddr,0,sizeof(localAddr));
		localAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
		localAddr.sin_family = AF_INET;
		localAddr.sin_port = htons(port);
        //Add localhost to known routes to prevent broadcast duplication/floods
        uint64_t sl;
        sl = localAddr.sin_addr.s_addr;
        sl |= ((uint64_t)localAddr.sin_port) << 32;
        routers[sl] = true;
        if(bind(server,(struct sockaddr*)&serverAddr,mlen) == -1) {
			//Bind failed
			//TODO: We possibly need to do a sendto on this socket to open
			//the correct port.
			unsigned char mbuff[2];

			sendto(server,mbuff,2,0,(struct sockaddr*)&broadcastAddr,sizeof(broadcastAddr));


		}else {
			//Join the multicast group and HAVE A PARTY!
			setsockopt(server,IPPROTO_IP,IP_ADD_MEMBERSHIP,&multicast,sizeof(multicast));
		}

		mtr = new std::thread([=](){
			while(running) {
				unsigned char buffy[1024*50];
				struct sockaddr_in clientaddr;
				clientaddr.sin_family = AF_INET;
				socklen_t len = sizeof(clientaddr);
				memset(&clientaddr,0,sizeof(clientaddr));

				auto received = recvfrom(server,buffy,1024*50,0,(struct sockaddr*)&clientaddr,&len);
                if(!running) {
                    break;
                }
                if(received>0) {
					//Process packet
                    uint64_t addr = (uint64_t)(clientaddr.sin_addr.s_addr);
                    uint64_t port = (uint64_t)(clientaddr.sin_port);
					uint64_t val = 0;
					//Fill first 32-bits
                    val = addr;
					//Fill second 32-bits
                    val |= port << 32;
                    if(routers.find(val) == routers.end()) {
                        //Update routing table
                        routers[val] = true;

                        knownRoutes.push_back(clientaddr);
                        fseek(fptr,0,SEEK_SET);
                        uint32_t count = (uint32_t)knownRoutes.size();
                        fwrite(&count,1,4,fptr);
                        fseek(fptr,0,SEEK_END);
                        fwrite(&val,1,8,fptr);
                    }

					std::lock_guard<std::recursive_mutex> l(mtx);
					if(socketMappings.find(val) == socketMappings.end()) {
						//printf("IP: Socket alloc MEMORY LEAK\n");
						//Microsoft Linux! (who would have EVER thought?)
						::VSocket* ms = GlobalGrid_AllocSocket();
                        InternetSocket* realConnection = new InternetSocket(server,clientaddr,this);
						ManagedToNativeSocket(ms,realConnection);
						//Docket mappings update
						socketMappings[val] = ms;
						GlobalGrid_ntfyPacket(mngr->nativePtr,ms,buffy,received);

					}else {
						//printf("IP: Lookup success (cache hit)\n");
						GlobalGrid_ntfyPacket(mngr->nativePtr,socketMappings[val],buffy,received);
					}
				}
			}
		});
	}
	~InternetProtocol() {
		running = false;
        close(server);
		mtr->join();
		delete mtr;
        fclose(fptr);
	}
};
}
#endif
