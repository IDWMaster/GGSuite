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



 // GlobalGrid.cpp : Defines the exported functions for the DLL application.
//
#ifdef WIN32
#include <Windows.h>
#endif
#include <stdint.h>
#include <map>
#include <mutex>
#include <vector>
#include <memory>
#include <time.h>
#include <thread>
#include <iostream>
#include <queue>
#include <condition_variable>

#include "GlobalGrid.h"
class IDisposable {
public:
	virtual ~IDisposable(){};
};
class Guid {
public:
	uint64_t value[2];
	bool operator<(const Guid& other) const {
		return memcmp(other.value,value,16)>0;
	}
	Guid() {

	}
	Guid(const unsigned char* id) {
		memcpy(value, id, 16);
	}
	void Write(unsigned char* dest) {
		memcpy(dest, value, 16);
	}

};
class SafeBuffer {
private:
	std::vector<unsigned char> rawData;
public:
	SafeBuffer(unsigned char* data, size_t sz) {
		rawData.resize(sz);
		memcpy(rawData.data(), data, sz);
		position = 0;
	}
	SafeBuffer(size_t sz) {
		rawData.resize(sz);
		position = 0;
	}
	SafeBuffer() {
		position = 0;
	}
	size_t position;
	void ReadBytes(void* dest, size_t sz) {
		if (position + sz <= rawData.size()) {
			memcpy(dest, rawData.data() + position, sz);
			position += sz;
		}
        else {
			throw std::exception();
		}
	}
	void WriteBytes(const void* src, size_t sz) {
		if (position + sz <= rawData.size()) {
			memcpy(rawData.data() + position,src, sz);
			position += sz;
		}
		else {
			throw std::exception();
		}
	}
	template<typename T>
	void Write(const T& obj) {
		WriteBytes(&obj, sizeof(obj));
	}
	template<typename T>
	void Read(T& dest) {
		ReadBytes(&dest, sizeof(dest));
	}
	size_t getAvailable() const {
		return rawData.size() - position;
	}
	size_t getSize() const {
		return rawData.size();
	}
    const unsigned char* getPointer() const {
        return rawData.data()+position;
    }
	unsigned char* getPointer() {
		return rawData.data() + position;
	}
};
typedef struct {
	//Source hardware address
	unsigned char SourceHW[16];
	//Source address
	unsigned char Source[16];
	//Destination address
	unsigned char Destination[16];
	//Time to live
	unsigned char ttl;

} PacketHeaders;
static void destroyFloodSocket(void* thisptr) {
	delete (std::vector<ProtocolDriver>*)thisptr;
}
static void sendFloodSocket(void* thisptr, unsigned char* data, size_t sz) {
	auto ptrList = (std::vector<ProtocolDriver>*)thisptr;
	std::vector<ProtocolDriver>& mlist = *ptrList;
	for (size_t i = 0; i < mlist.size(); i++) {
		mlist[i].sendBroadcast(mlist[i].thisptr, data, sz);
	}
}
static void createFloodSocket(std::vector<ProtocolDriver>&& drivers, VSocket* socket) {
	std::vector<ProtocolDriver>* driverList = new std::vector<ProtocolDriver>(drivers);
	socket->Destroy = destroyFloodSocket;
	socket->Send = sendFloodSocket;
	socket->thisptr = driverList;
	
}
class IntelligentRoute {
public:
	std::recursive_mutex mtx;
	time_t constructionTime;
	void Initialize() {
		time(&constructionTime);
	}
	unsigned char router[16];
	VSocket* ptr;
	std::shared_ptr<IntelligentRoute> alternate;
	VSocket*& operator->() {
		return ptr;
		
	}
	IntelligentRoute(VSocket* ptr) {
		this->ptr = ptr;
		
		Initialize();
	}
	
	bool isOverdue() {
		time_t current;
		time(&current);
		return difftime(current,constructionTime) > 15;
	}
	void Renew() {
		time(&constructionTime);
	}
	operator VSocket*&() {
		time(&constructionTime);
		return ptr;
	}
	~IntelligentRoute() {
		//TODO: This is causing the WHOLE problem, ALSO this is not ABI-safe, because
        //the VSocket* MAY have been allocated in a different application space, so we
        //cannot safely de-allocate its memory. It is the responsibility of the
        //implementor to delete the ptr when Destroy is called.
		//NOTE: If this is NOT handled properly, it could result in a memory leak
		//TODO: Solution -- All VSockets should be allocated by the GlobalGrid library
		//NEVER by the application, using GlobalGrid_AllocSocket
		if (ptr->Destroy != 0) {
			ptr->Destroy(ptr->thisptr);
		}
		ptr = 0;
		delete ptr;
	}
};
extern "C" VSocket* GlobalGrid_AllocSocket() {
	return new VSocket();
}
static std::map<VSocket*, std::weak_ptr<IntelligentRoute>> liveRoutes;
static std::recursive_mutex liveRouteMtx;
static std::shared_ptr<IntelligentRoute> VSOCKET(VSocket* ptr) {

	std::shared_ptr<IntelligentRoute> val;
	liveRouteMtx.lock();
	if (liveRoutes.find(ptr) == liveRoutes.end()) {
		val = std::make_shared<IntelligentRoute>(ptr);
		liveRoutes[ptr] = val;
	}
	else {
		if (val = liveRoutes[ptr].lock()) {

		}
		else {
			val = std::make_shared<IntelligentRoute>(ptr);
			liveRoutes[ptr] = val;
		}
	}
	liveRouteMtx.unlock();
	return val;
}
class P2PConnectionManager:public IDisposable {
public:
	void* dbHandle;
	unsigned char broadcastAddress[16];
	unsigned char id[16];
	std::map<Guid, ProtocolDriver> protocolDrivers;
	std::map<Guid, std::shared_ptr<IntelligentRoute>> cachedRoutes;
	std::set<Guid> frozenRoutes;
	std::recursive_mutex mtx;
	std::shared_ptr<IntelligentRoute> flood;
	unsigned char local[16];
	volatile bool running;
	const unsigned char maxTTL = 50;
	std::thread gcthread;
	size_t currentPort;
	std::queue<size_t> availablePorts;
	void FreezeRoute(Guid g) {
	  mtx.lock();
	  frozenRoutes.insert(g);
	  mtx.unlock();
	}
	P2PConnectionManager() {
		currentPort = 1000;
		running = true;
		OS_GetSystemGuid(id);
		memset(&broadcastAddress, 255, 16);
		mtx.lock();
		std::vector<ProtocolDriver> drivers;
		for (auto it = protocolDrivers.begin(); it != protocolDrivers.end(); it++) {
			drivers.push_back(it->second);
		}
		mtx.unlock();
		memset(local, 0, 16);
		VSocket* flood = new VSocket();
		createFloodSocket(std::move(drivers), flood);
		this->flood = std::make_shared<IntelligentRoute>(flood);
		liveRoutes[flood] = this->flood;
		gcthread = std::move(std::thread([=](){
			PacketHeaders header;
			memcpy(header.Destination, broadcastAddress, 16);
			memcpy(header.SourceHW, id, 16);
			memcpy(header.Source, id, 16);
			header.ttl = 1;
			unsigned char mpacket[sizeof(header)+1];
			memcpy(mpacket, &header, sizeof(header));
			mpacket[sizeof(header)] = 0;
			while (this->running) {
				OS_Sleep(5);
				mtx.lock();
				std::vector<std::shared_ptr<IntelligentRoute>> collectedRoutes;
				std::vector<Guid> routeGuids;
				//Collect garbage
				for (auto it = cachedRoutes.begin(); it != cachedRoutes.end(); it++) {
					if (it->second->isOverdue()) {
						if (it->second->alternate) {
							collectedRoutes.push_back(it->second);
							routeGuids.push_back(it->first);
						}
					}
				}
				//Erase routes from table
				for (size_t i = 0; i < routeGuids.size(); i++) {
					printf("Route has expired -- DO SOMETHING HERE\n");
					
				}

				mtx.unlock();
				

				
				//Send broadcast
				this->flood->ptr->Send(this->flood->ptr->thisptr, mpacket, sizeof(header)+1);

			}
		}));
	}
	std::shared_ptr<IntelligentRoute> Deserialize(SafeBuffer& data) {
		unsigned char protoID[16];
		data.Read(protoID);
		bool found = false;
		ProtocolDriver driver;
		mtx.lock();
		if (protocolDrivers.find(protoID) != protocolDrivers.end()) {
			found = true;
			driver = protocolDrivers[protoID];

		}
		mtx.unlock();
		if (!found) {
            //Protocol driver not found
			throw std::exception();
		}
		VSocket* msock = new VSocket();
		bool success = driver.Deserialize(driver.thisptr, data.getPointer(), data.getAvailable(), msock);
		std::shared_ptr<IntelligentRoute> retval = VSOCKET(msock);
		if (success) {
			return retval;
		}
		else {
            //Protocol driver not found
			throw std::exception();
		}
	}
	std::shared_ptr<IntelligentRoute> findRoute(Guid destination) {
		//Search for cached routes in routing table
		
		mtx.lock();
		
		bool hasroute = false;
		std::shared_ptr<IntelligentRoute> cachedRoute;
		if (cachedRoutes.find(destination) != cachedRoutes.end()) {
			cachedRoute = cachedRoutes[destination];
			hasroute = true;
		}
		mtx.unlock();
		if (hasroute) {
			return cachedRoute;
		}
		//No cached routes found -- search database
        return flood;
		}
        



	void addRoute(Guid destination, Guid router, std::shared_ptr<IntelligentRoute> route) {
		mtx.lock();
		if(frozenRoutes.find(destination) == frozenRoutes.end()) {
		if (cachedRoutes[destination] != route) {
			cachedRoutes[destination] = route;
		}

		memcpy(cachedRoutes[destination]->router, router.value, 16);
		}
		mtx.unlock();
		
	}  
	std::map<int32_t, ReceiveCallback> openPorts;
	typedef struct {
		PacketHeaders headers; 
		unsigned char opcode;
	} GlobalGridPacket;
	typedef struct SerializedRoutePacket {
		GlobalGridPacket globalGrid;
		unsigned char routeID[16];
		unsigned char protoID[16];
	};
	void ntfyPacket(unsigned char* data, size_t packetSz, VSocket* m_socket) {
		//Here's what appears to be happening:
		//This thread (on which ntfyPacket is called)
		//receives a message on a VSocket.
		//This VSocket's reference count is decremented to zero (0) on another thread.
		//The VSocket begins disposing (memory no longer valid)
		//This thread tries to access the (now invalid) VSocket, causing a race condition
		std::shared_ptr<IntelligentRoute> socket = VSOCKET(m_socket);

		std::shared_ptr<IntelligentRoute> fromSocket = socket;
		if (socket->ptr->thisptr == 0) {
			//Race condition detected
			printf("PROTO_DRIVER--ERR: Race condition detected\n");
			return;
		}
		if(packetSz < 4) {
			//Bad data from upstream provider
			return;
		}
		SafeBuffer stream(data, packetSz);
		try {
			//Address headers
			PacketHeaders headers;
			//TODO: Failing at header read. Is packet size 0?
			stream.Read(headers);
			if (memcmp(headers.Source, id, 16) == 0) {
				//Packet from ourselves -- ignore
				return;
			}

			if ((memcmp(headers.Destination, broadcastAddress, 16) == 0) || (memcmp(headers.Destination, id, 16) == 0)) {
				//Packet intended for our address (keep route alive)
				socket->Renew();
				unsigned char opcode;
				stream.Read(opcode);
				//Add route to routing table
				addRoute(headers.Source, headers.SourceHW, socket);

				switch (opcode) {
				case 0:
				{
						  //Routine broadcast (local) -- send an ICBM in response to this ICMP
						  GlobalGridPacket packet;
						  PacketHeaders& icbmHeaders = packet.headers;
						  icbmHeaders.ttl = 1;
						  memcpy(icbmHeaders.Destination, headers.Source, 16);
						  memcpy(icbmHeaders.Source, id, 16);
						  memcpy(icbmHeaders.SourceHW, id, 16);
						  packet.opcode = 3;
						  socket->ptr->Send(socket->ptr->thisptr, (unsigned char*)&packet, sizeof(packet));
				}
					break;
				case 1:
					//Data received
				{
						  int32_t portno;
						  stream.Read(portno);
						  int32_t srcPort;
						  stream.Read(srcPort);
						  mtx.lock();
						  bool found = false;
						  ReceiveCallback callback;
						  if (openPorts.find(portno) != openPorts.end()) {
							  found = true;
							  callback = openPorts[portno];
						  }  
						  mtx.unlock();
						  if (found) {
							  size_t dataLen = stream.getAvailable();
 
							  unsigned char* dataPortion = new unsigned char[dataLen];
							  stream.ReadBytes(dataPortion, dataLen);
							  callback.onReceived(callback.thisptr, headers.Source,srcPort, dataPortion, dataLen);
							  delete[] dataPortion;
						  }
				}
					break;
				case 2:
				{
						  //Serialized route found
						  try {
							  unsigned char peerID[16];
							  stream.Read(peerID);
							  auto val = Deserialize(stream);
							  addRoute(peerID, peerID, val);
						  }
						  catch (std::exception er) {
						  }
				}
					break;
				case 3:
				{
						  //ICBM
				}
					break;
				}
			}
			else {
				//Find route to intended destination
				if (headers.ttl > 0 && stream.getAvailable()>0) {
					headers.ttl--;
					unsigned char* newpacket = new unsigned char[sizeof(headers)+stream.getAvailable()];
					PacketHeaders newHeaders = headers;
					memcpy(newHeaders.SourceHW, id, 16);
					memcpy(newpacket, &newHeaders, sizeof(newHeaders));
					stream.ReadBytes(newpacket + sizeof(headers), stream.getAvailable());
					std::shared_ptr<IntelligentRoute> socket = findRoute(headers.Destination);
					mtx.lock();
					socket->ptr->packetsSent++;
					mtx.unlock();
					//Check for direct route BEFORE sending the actual datagram (for low-latency virtual networks in which packets are delivered in realtime)
					//this may make a BIG difference on such networks; otherwise both peers will have "unaligned" routes.
					//(in other words; although a direct connection may be possible if both systems are on the same
					//logical network, one will assume a direct connection while the other attempts to route
					//through us. Assuming optimistic in-order packet delivery occurs; this should minimize the
					//number of times that happens, and place less strain on the routing infrastructure)
					if (memcmp(socket->router, headers.Destination, 16) == 0 && memcmp(headers.SourceHW, headers.Source, 16) == 0) {
						//TODO: Send direct routing data back to source AND destination (so they directly connect to each other)

						PacketHeaders r_headers;
						memcpy(r_headers.Source, id, 16);
						memcpy(r_headers.SourceHW, id, 16);
						memcpy(r_headers.Destination, headers.Source, 16);
						r_headers.ttl = 1;
						void* output;
						socket->ptr->Serialize(socket->ptr->thisptr, &output);
						ByteArray mray = OS_ResolveByteArray(output);
						SafeBuffer buffer(mray.data, mray.size);
						OS_Free(output);
						//Serialized stream
						size_t bufsz = sizeof(r_headers)+1 + 16 + buffer.getAvailable() + 16;
						SafeBuffer data(bufsz);
						data.Write(r_headers);
						*data.getPointer() = 2;
						data.position++;
						//ID of peer
						data.Write(headers.Destination);
						socket->ptr->getProtoID(socket->ptr->thisptr, data.getPointer());
						data.position += 16;
						buffer.ReadBytes(data.getPointer(), buffer.getAvailable());
						data.position = 0;
						//Send to the computer requesting the route
						fromSocket->ptr->Send(fromSocket->ptr->thisptr, data.getPointer(), bufsz);
						//Send to the computer that is the DESTINATION of the route
						fromSocket->ptr->Serialize(fromSocket->ptr->thisptr, &output);
						mray = OS_ResolveByteArray(output);
						//The serialized data will be stored here
						buffer = SafeBuffer(mray.data, mray.size);
						OS_Free(output);
						//Set packet headers
						SerializedRoutePacket frame;
						memcpy(frame.globalGrid.headers.Destination, headers.Destination, 16);
						memcpy(frame.globalGrid.headers.Source, id, 16);
						memcpy(frame.globalGrid.headers.SourceHW, id, 16);
						frame.globalGrid.headers.ttl = 1;
						frame.globalGrid.opcode = 2;
						memcpy(frame.routeID, headers.Source,16);
						fromSocket->ptr->getProtoID(fromSocket->ptr->thisptr, frame.protoID);
						data = SafeBuffer(sizeof(frame)+buffer.getSize());
						data.Write(frame);
						buffer.ReadBytes(data.getPointer(), buffer.getSize());
						data.position = 0;
						socket->ptr->Send(socket->ptr->thisptr, data.getPointer(), data.getSize());
						
					}
					
					
					
					socket->ptr->Send(socket->ptr->thisptr, newpacket, stream.position);
					
					delete[] newpacket;
					
				}
			}
		}
		catch (const std::exception& er) {

			//std::cout <<"GGERR: "<< er.what()<<std::endl;
			//throw er;
		}
	}
	void RegisterProtocolDriver(Guid protoID, ProtocolDriver driver) {
		mtx.lock();
		protocolDrivers[protoID] = driver;
		mtx.unlock();
		mtx.lock();
		std::vector<ProtocolDriver> drivers;
		for (auto it = protocolDrivers.begin(); it != protocolDrivers.end(); it++) {
			drivers.push_back(it->second);
		}
		VSocket* flood = new VSocket();
		createFloodSocket(std::move(drivers), flood);
		this->flood = VSOCKET(flood);
		mtx.unlock();
	}
	void Send(Guid dest, int32_t portno, int32_t srcportno, SafeBuffer data) {
		PacketHeaders headers;
		memcpy(headers.SourceHW, id, 16);
		//Packet headers, opcode, dest portno, src portno,  data
		size_t length = sizeof(PacketHeaders)+1+4 +4+ data.getAvailable();
		unsigned char* packet = new unsigned char[length];
		packet[sizeof(PacketHeaders)] = 1;
		memcpy(headers.Destination, dest.value, 16);
		memcpy(headers.Source, id, 16);
		headers.ttl = maxTTL;
		memcpy(packet, &headers, sizeof(headers));
		memcpy(packet + sizeof(headers)+1, &portno, 4);
		memcpy(packet + sizeof(headers)+1 + 4, &srcportno, sizeof(srcportno));
		data.ReadBytes(packet + sizeof(headers)+1+4+4, data.getAvailable());
		std::shared_ptr<IntelligentRoute> socket = findRoute(dest);
		mtx.lock();
		socket->ptr->packetsSent++;
		mtx.unlock();
		socket->ptr->Send(socket->ptr->thisptr,packet,length);
	}
	~P2PConnectionManager() {
		running = false;
		gcthread.join();
		for (auto it = protocolDrivers.begin(); it != protocolDrivers.end(); it++) {
			it->second.shutdownHandler(it->second.thisptr);
		}
	}
};
void* createP2PConnectionManager() {
	
	return new P2PConnectionManager();
}
void FreePtr(void* libHandle) {
	delete (IDisposable*)libHandle;
}
void GlobalGrid_GetID(void* connectionManager, unsigned char* id) {
	P2PConnectionManager* mngr = (P2PConnectionManager*)connectionManager;
	memcpy(id, mngr->id, 16);
}
void GlobalGrid_RegisterProtocol(void* connectionManager, unsigned char* id,ProtocolDriver driver) {
	Guid mid;
	memcpy(&mid.value, id, 16);
	((P2PConnectionManager*)connectionManager)->RegisterProtocolDriver(mid, driver);
}
void GlobalGrid_ntfyPacket(void* connectionManager, VSocket* socket, unsigned char* data, size_t sz) {
	((P2PConnectionManager*)connectionManager)->ntfyPacket(data, sz,socket);
}
void GlobalGrid_OpenPort(void* connectionManager,int32_t portno, ReceiveCallback onReceived) {
	auto mngr = ((P2PConnectionManager*)connectionManager);
	mngr->mtx.lock();
	mngr->openPorts[portno] = onReceived;
	mngr->mtx.unlock();
}
void GlobalGrid_ClosePort(void* connectionManager, int32_t portno) {
	auto mngr = ((P2PConnectionManager*)connectionManager);
	mngr->mtx.lock();
	void* thisptr = mngr->openPorts[portno].thisptr;
	auto destroyCallback = mngr->openPorts[portno].onDestroyed;
	mngr->openPorts.erase(portno);
	mngr->mtx.unlock();
	destroyCallback(thisptr); 
}
void GlobalGrid_Send(void* connectionManager, unsigned char* dest, int32_t destportno, int32_t srcportno, unsigned char* data, size_t sz) {
	auto mngr = ((P2PConnectionManager*)connectionManager);
	SafeBuffer buffer(data,sz);
	mngr->Send(dest, destportno,srcportno, buffer);
}
void GlobalGrid_FreezeSocket(void* connectionManager,unsigned char* guid) {
  auto mngr = ((P2PConnectionManager*)connectionManager);
  mngr->mtx.lock();
  mngr->FreezeRoute(Guid(guid));
  mngr->mtx.unlock();
}

size_t GlobalGrid_GetPeerList(void* connectionManager, GlobalGrid_Identifier** list) {
	P2PConnectionManager* manager = (P2PConnectionManager*)connectionManager;
	size_t retval;
	manager->mtx.lock();
	if (manager->cachedRoutes.size() == 0) {
		*list = 0;
		retval = 0;
	}
	else {
		*list = new GlobalGrid_Identifier[manager->cachedRoutes.size()];
		retval = manager->cachedRoutes.size();
		size_t idx = 0;
		for (auto i = manager->cachedRoutes.begin(); i != manager->cachedRoutes.end(); i++) {
			memcpy((*list)[idx].value, i->first.value, 16);
			idx++;
		}

	}
	manager->mtx.unlock();
	return retval;
}
void GlobalGrid_FreePeerList(GlobalGrid_Identifier* list) {
	if (list != 0) {
		delete[] list;
	}
}
