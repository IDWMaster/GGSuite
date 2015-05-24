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




//
//  GlobalGrid.h
//  GlobalGrid
//
//  Created by owner on 6/14/14.
//  Copyright (c) 2014 IDWNet Cloud Computing. All rights reserved.
//

#ifndef GLOBALGRID_H
#define GLOBALGRID_H


//#import <UIKit/UIKit.h>





#include <iostream>
#include <unistd.h>

#include <cmath>
#include <algorithm>
#include <stdint.h>
#include <string.h>
#include <memory>
#include <map>
const uint32_t segmentLength = 1024*4;
#include "LightThread.h"
//PLATFORM-NATIVES
typedef struct {
    unsigned char* data;
    size_t size;
} ByteArray;
typedef struct {
    unsigned char dest[16];
    unsigned char router[16];
    ByteArray data;
} Route;
typedef struct {
    void(*Send)(void* thisptr, unsigned char* data, size_t sz);
    void(*Destroy)(void* thisptr);
    void(*Serialize)(void* thisptr, void** byteBufferOutput);
    void(*getProtoID)(void* thisptr,unsigned char* output);
    void* thisptr;
    size_t packetsSent;
    
} VSocket;
typedef struct {
    //Sends a broadcast message over the specified protocol
    void(*sendBroadcast)(void* thisptr, unsigned char* data, size_t sz);
    //Called when this Protocol Driver is to be removed from the networking stack
    void(*shutdownHandler)(void* thisptr);
    bool(*Deserialize)(void* thisptr,unsigned char* data, size_t len,VSocket* socket);
    void* thisptr;
} ProtocolDriver;
typedef struct {
    void* thisptr;
    void(*onDestroyed)(void* thisptr);
    void(*onReceived)(void* thisptr, unsigned char* src, int32_t srcPort, unsigned char* data, size_t sz);
} ReceiveCallback;
typedef struct {
    int64_t value[2];
} GlobalGrid_Identifier;
#ifdef __cplusplus
extern "C" {
#endif
    void OS_GetSystemGuid(unsigned char* id);
    void OS_Free(void* obj);
    void OS_Sleep(int seconds);
    Route* getRoutesFromHandle(void* routeHandle);
    void* createP2PConnectionManager();
    void FreePtr(void* libHandle);
    VSocket* GlobalGrid_AllocSocket();
    void GlobalGrid_GetID(void* connectionManager, unsigned char* id);
    void GlobalGrid_RegisterProtocol(void* connectionManager, unsigned char* id,ProtocolDriver driver);
    void GlobalGrid_ntfyPacket(void* connectionManager, VSocket* socket, unsigned char* data, size_t sz);
    void GlobalGrid_OpenPort(void* connectionManager,int32_t portno,ReceiveCallback onReceived);
    void GlobalGrid_ClosePort(void* connectionManager,int32_t portno);
    void GlobalGrid_Send(void* connectionManager, unsigned char* dest,int32_t srcportno,int32_t destportno,unsigned char* data, size_t sz);
    size_t GlobalGrid_GetPeerList(void* connectionManager,GlobalGrid_Identifier** list);
    void GlobalGrid_FreePeerList(GlobalGrid_Identifier* list);
    ByteArray OS_ResolveByteArray(void* byteArray);

#ifdef __cplusplus
}




//C++-ABI
namespace GlobalGrid {
class IDisposable {
public:
	virtual ~IDisposable(){};
};
static void ABI_Free(void* ptr) {
	delete ((IDisposable*)ptr);
}
class SafeBuffer:public IDisposable {
public:
	unsigned char* data;
	size_t len;
	SafeBuffer(unsigned char* data, size_t len) {
		this->data = new unsigned char[len];
		memcpy(this->data,data,len);
		this->len = len;
	}
	SafeBuffer(){
		this->data = 0;
		len = 0;
	}
	~SafeBuffer() {
		if(data) {
		delete[] data;
		}
	}
};


class VSocket:public IDisposable {
public:
	virtual void Send(SafeBuffer& buffer) const = 0;
	virtual void Serialize(SafeBuffer& output) const = 0;
	unsigned char protoID[16];
};


class ProtocolDriver {
public:
	unsigned char protoID[16];
	virtual VSocket* Deserialize(const SafeBuffer& data) = 0;
	virtual void Broadcast(const SafeBuffer& packet) const = 0;
	virtual ~ProtocolDriver(){};
};
static void ABI_Send(void* thisptr, unsigned char* data, size_t sz) {
	VSocket* msock = (VSocket*)thisptr;
	SafeBuffer mb(data,sz);
	msock->Send(mb);
}
static void ABI_Serialize(void* thisptr, void** output) {
	VSocket* msock = (VSocket*)thisptr;
	SafeBuffer* mb = new SafeBuffer();
	msock->Serialize(*mb);

}
static void ABI_GetProtoID(void* thisptr, unsigned char* output) {
	VSocket* msock = (VSocket*)thisptr;
	memcpy(output,msock->protoID,16);
}
static void ManagedToNativeSocket(::VSocket* dest, VSocket* src) {
	::VSocket* sockptr = dest;
	sockptr->Destroy = ABI_Free;
	sockptr->Send = ABI_Send;
	sockptr->Serialize = ABI_Serialize;
	sockptr->getProtoID = ABI_GetProtoID;
	sockptr->thisptr = src;
	sockptr->packetsSent = 0;

}

static bool ABI_Deserialize(void* thisptr, unsigned char* data, size_t len, ::VSocket* sockptr) {
	ProtocolDriver* deriver = (ProtocolDriver*)thisptr;
	VSocket* retval = deriver->Deserialize(SafeBuffer(data,len));
	ManagedToNativeSocket(sockptr,retval);
	if(retval == 0) {
		return false;
	}
	return true;
}
static void ABI_Broadcast(void* thisptr, unsigned char* data, size_t sz) {
	//Send boradcast on ProtocolDriver
	auto protomanager = (ProtocolDriver*)thisptr;
	protomanager->Broadcast(SafeBuffer(data,sz));

}
static void ABI_Shutdown(void* thisptr) {
	ProtocolDriver* deriver = (ProtocolDriver*)thisptr;
	delete deriver;
}
class P2PConnectionManager {
public:
	void* nativePtr;
	P2PConnectionManager() {
		nativePtr = createP2PConnectionManager();
	}
	~P2PConnectionManager() {
		FreePtr(nativePtr);
	}
	void getID(unsigned char* output) {
		GlobalGrid_GetID(nativePtr,output);
	}
	void RegisterProtocol(ProtocolDriver* deriver) {
		::ProtocolDriver nativeobj;
		nativeobj.Deserialize = ABI_Deserialize;
		nativeobj.sendBroadcast = ABI_Broadcast;
		nativeobj.shutdownHandler = ABI_Shutdown;
		nativeobj.thisptr = deriver;
		GlobalGrid_RegisterProtocol(nativePtr,deriver->protoID,nativeobj);
	}
};


//RELIABILITY LAYER

class GGManager_Forward {
public:
	std::shared_ptr<P2PConnectionManager> manager;
	virtual ~GGManager_Forward(){};
};



class ReliableSocket {
public:
	unsigned char dest[16];
	int32_t srcPort;
	int32_t destPort;
	uint16_t windowSize;
	size_t retryInterval;
	size_t retries;
	uint16_t frameID;
	GGManager_Forward* mngr;

	//RECV buffers
	unsigned char* recv_data;
	bool* recv_b;
	size_t recv_dataSz;
	ReliableSocket(const unsigned char* dest, const int32_t& srcPort, const int32_t& destPort, GGManager_Forward* mngr) {
		recv_dataSz = 0;
		recv_data = 0;
		recv_b = 0;
		this->mngr = mngr;
		memcpy(this->dest,dest,16);
		this->srcPort = srcPort;
		this->destPort = destPort;
		windowSize = 1024;
		retryInterval = 200;
		retries = 10;
		frameID = 0;
	}
	std::mutex mtx;
	std::function<void()> completionCallback;
	template<typename F>
	void Send(const unsigned char* user_data, size_t sz, const F& user_completionHandler) {
		//Fragment packet
		unsigned char* data = new unsigned char[sz];
		memcpy(data,user_data,sz);
		size_t fragCount = (sz / windowSize);
		if(fragCount*windowSize != sz) {
			fragCount++;
		}
		auto cleanup = [=](){
			delete[] data;
		};

		uint16_t fid_int = frameID;
		auto transmit = [=](std::function<void()> cb){
			mtx.lock();
			completionCallback = [=](){
				cb();
				user_completionHandler(true);
			};
			size_t remaining = sz;
			unsigned char* dptr = data;
			for(size_t i = 0;i<fragCount;i++) {
				//Length (4)
				//Window size (2)
				//Fragment ID (2)
				//Frame ID (2)
				//Data (arbitrary length)
				size_t avail = std::min(remaining,(size_t)windowSize);
				unsigned char* dgram = new unsigned char[1+4+2+2+2+avail];
				dgram[0] = 0;
				memcpy(dgram+1,&sz,4);
				memcpy(dgram+1+4,&windowSize,2);
				memcpy(dgram+1+4+2,&i,2);
				memcpy(dgram+1+4+2+2,&fid_int,2);
				memcpy(dgram+1+4+2+2+2,dptr,avail);
				GlobalGrid_Send(mngr->manager->nativePtr,dest,srcPort,destPort,dgram,1+4+2+2+2+avail);
				delete[] dgram;
				dptr+=avail;
			}
			mtx.unlock();


		};
		RetryOperation(transmit,retryInterval,retries,[=](){
				cleanup();
				user_completionHandler(false);
		});

	}
	~ReliableSocket() {
		//TODO: Destructor
		if(recv_data) {
			delete[] recv_b;
			delete[] recv_data;
		}
	}
};


class ReliableSocketMapping {
public:
	unsigned char mander[16+4];
	ReliableSocketMapping(const unsigned char* guid, const int32_t& port) {
		memcpy(mander,guid,16);
		memcpy(mander+16,&port,4);
	}
	bool operator<(const ReliableSocketMapping& other) const {
		return memcmp(mander,other.mander,20) < 0;
	}
};

static void reliable_recv(void* thisptr, unsigned char* src, int32_t srcPort, unsigned char* data, size_t sz);
class ReliableConnectionManager:public GGManager_Forward {
public:
	std::mutex mtx;
	std::map<ReliableSocketMapping,std::weak_ptr<ReliableSocket>> connections;
int32_t portno;
	std::function<void(unsigned char*,size_t,std::shared_ptr<ReliableSocket>)> callback;
	template<typename F>
	ReliableConnectionManager(std::shared_ptr<P2PConnectionManager> manager, int32_t portno, const F& callback) {
		this->manager = manager;
		ReceiveCallback cb;
		cb.thisptr = this;
		cb.onReceived = reliable_recv;
		this->portno = portno;
		this->callback = callback;
		GlobalGrid_OpenPort(manager->nativePtr,portno,cb);
	}
	std::shared_ptr<ReliableSocket> Connect(unsigned char* id, int32_t portno) {
		//dest, srcport, destport, this
		std::shared_ptr<ReliableSocket> retval = std::make_shared<ReliableSocket>(id,this->portno,portno,this);
		ReliableSocketMapping key(id,portno);
		mtx.lock();
		connections[key] = retval;
		mtx.unlock();
		return retval;
	}
	~ReliableConnectionManager() {
		//TODO: DESTROY!
	}
};
static void reliable_recv(void* thisptr, unsigned char* src, int32_t srcPort, unsigned char* data, size_t sz) {

	//RELIABLE FRAME
	//1 byte OPCODE
	//Total Length (4 bytes) -- The length of this frame
	//Window size (2 bytes)
	//Fragment ID (2 bytes)
	//Frame ID (2 bytes)
	if(sz<1+4+2+2+2) {
		return;
	}
	ReliableConnectionManager* mngr = (ReliableConnectionManager*)thisptr;
	//TODO: Reliable packet received
	ReliableSocketMapping searchDescriptor(src,srcPort);
	//Find connection, if one exists
	std::shared_ptr<ReliableSocket> foundSocket;
	mngr->mtx.lock();
	if(mngr->connections.find(searchDescriptor) != mngr->connections.end()) {
		foundSocket = mngr->connections[searchDescriptor].lock();
	}
	if(!foundSocket) {
		foundSocket = std::make_shared<ReliableSocket>(src,mngr->portno,srcPort,mngr);
		mngr->connections[searchDescriptor] = foundSocket;
	}
	auto sendACK = [=](uint16_t frameID){
		//In an ACKFlack (please don't ask about it at work)
		//the whole packet structure is sent, but only the first 3 bytes are
		//important.
		//ACK packet
		//Opcode (1), frame ID (2)
		unsigned char packet[1+4+2+2+2];
		//Don't want to expose any data inadvertently
		memset(packet,0,1+4+2+2+2);
		packet[0] = 1;
		memcpy(packet+1,&frameID,2);
		GlobalGrid_Send(mngr->manager->nativePtr,src,mngr->portno,srcPort,packet,1+4+2+2+2);
	};
	mngr->mtx.unlock();

	switch(*data) {
	case 0:
	{
		data++;
		//Data packet
		uint32_t len;
		uint16_t windowSz;
		uint16_t fragID;
		uint16_t frameID;
		memcpy(&len,data,4);
		data+=4;
		sz-=4;
		memcpy(&windowSz,data,2);
		data+=2;
		sz-=2;
		memcpy(&fragID,data,2);
		data+=2;
		sz-=2;
		memcpy(&frameID,data,2);
		data+=2;
		sz-=2;
		size_t fragCount = len/windowSz;
		if(fragCount * windowSz != len) {
			fragCount++;
		}
		if(fragID >=fragCount) {
			//Bufer overflow attack -- mitigated
			return;
		}
		foundSocket->mtx.lock();
		//Verify frame ID
		if(foundSocket->frameID == frameID) {

			velociraptor:
			if(foundSocket->recv_data == 0) {
				//TODO: DOS potential (exhaustion of virtual address space) (per-process limit)
				foundSocket->recv_data = new unsigned char[len];
				foundSocket->recv_b = new bool[fragCount];
				foundSocket->recv_dataSz = len;
				memset(foundSocket->recv_b,0,fragCount);
				memset(foundSocket->recv_data,0,len);

			}
			if(foundSocket->recv_dataSz != len) {
				delete[] foundSocket->recv_data;
				delete[] foundSocket->recv_b;
				foundSocket->recv_data = 0;
				foundSocket->recv_b = 0;

				goto velociraptor;
			}

			size_t avail = std::min((size_t)windowSz,sz);
			memcpy(foundSocket->recv_data+(fragID*windowSz),data,avail);
			foundSocket->recv_b[fragID] = true;
			bool complete = true;
			for(size_t i = 0;i<fragCount;i++) {
				if(foundSocket->recv_b[i] == false) {
					complete = false;
					break;
				}
			}
			if(complete) {
				//WE GOT PACKETS (notify)!!!!
				sendACK(frameID);

				//Unlock to prevent deadlock; in case user immediately transmits a packet again....


				mngr->callback(foundSocket->recv_data,len,foundSocket);

				foundSocket->frameID++;
				memset(foundSocket->recv_b,0,fragCount);
				foundSocket->mtx.unlock();
				memset(foundSocket->recv_data,0,len);
				foundSocket->mtx.lock();
			}else {
				mngr->callback(0,0,foundSocket);
			}
		}else {
			if(frameID == foundSocket->frameID-1) {
				//Re-send ACK
				sendACK(frameID);
			}
		}
		foundSocket->mtx.unlock();
	}
	break;
	case 1:
	{
		//Packet acknowledgement

		foundSocket->mtx.lock();
		data++;
		uint16_t frameID;
		memcpy(&frameID,data,2);
		if(frameID == foundSocket->frameID) {

			//We got ACK!
			foundSocket->frameID++;
			auto cb = foundSocket->completionCallback;
			foundSocket->mtx.unlock();
			SubmitWork(cb);
		}else {
			foundSocket->mtx.unlock();
		}
	}
		break;
	case 2:
	{
		//Currently unused -- reserved for future use
	}
		break;
	}

}
}
//END RELIABILITY LAYER

#endif
#endif
