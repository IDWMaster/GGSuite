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



// OpenAuth.h

#ifndef OpenNet_Auth
#define OpenNet_Auth

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include "LightThread.h"
typedef struct {
    //The authority who signed the blob
    char* authority;
    //The blob
    unsigned char* blob;
    //The length of the blob
    size_t bloblen;
    unsigned char* signature;
    size_t siglen;
} NamedObject;
typedef struct {
	char* authority;
	unsigned char* pubkey;
	size_t pubLen;
	unsigned char* signature;
	size_t siglen;
} OCertificate;
#ifdef __cplusplus
template<typename R, typename F, typename... args>
static R unsafe_c_callback(void* thisptr, args... a) {
	return (*((F*)thisptr))(a...);
}



template<typename F, typename... args, typename R>
static void* C(const F& callback, R(*&fptr)(void*, args...)) {
	fptr = unsafe_c_callback<R, F, args...>;
	return (void*)&callback;
}
extern "C" {
#endif
    void* OpenNet_OAuthInitialize();
    void AES_Encrypt(unsigned char* key, unsigned char* dest, unsigned char* src);
    void AES_Decrypt(unsigned char* key, unsigned char* dest, unsigned char* src);
    void OpenNet_Retrieve(void* db, const char* name, void* thisptr, void(*callback)(void* thisptr,NamedObject* obj));
    bool OpenNet_AddObject(void* db, const char* name, const NamedObject* obj);
    void OpenNet_OAuthDestroy(void* db);
    void OpenNet_OAuthEnumPrivateKeys(void* db, void* thisptr, bool(*callback)(void* thisptr,const char* thumbprint));
    void OpenNet_MakeObject(void* db, const char* name,  NamedObject* obj, bool update);
    void OpenNet_AddCertificate(void* db,const OCertificate* abi, void* thisptr, void(*callback)(void*,const char*));
    void OpenNet_RetrieveCertificate(void* db, const char* thumbprint,  void* thisptr,  void(*callback)(void*,OCertificate*));
    //Finds the trust anchor for a given certificate; as well as an indication of the resolution status
    void OpenNet_ResolveChain(void* db, const char* thumbprint, void* thisptr, void(*callback)(void*, const char*, bool));
    bool OpenNet_UpdateObject(void* db, const char* name, const NamedObject* obj);
    void OpenNet_AddDomain(void* db, const char* name, const char* parent, const char* objid);
    void OpenNet_FindReverseDomain(void* db, const char* objid, void* thisptr, void(*callback)(void*,const char*, const char*));
    void OpenNet_FindDomain(void* db, const char* domain, const char* parent, void* thisptr, void(*callback)(void*, const char*));
    void DMCA_TakedownBlob(void* db,const char* name);
    void OpenNet_BeginTransaction(void* db);
    void OpenNet_EndTransaction(void* db);
    bool OpenNet_VerifySignature(void* db, const char* authority, unsigned char* data, size_t sz, unsigned char* signature, size_t siglen);
    void OpenNet_SignData(void* db, const char* authority, unsigned char* data, size_t sz, void* thisptr, void(*callback)(void*,unsigned char*,size_t));
    void OpenNet_AddDomainPtr(void* db, const char* objid, const char* ptrObject);
    void OpenNet_RetrieveDomainPtr(void* db, const char* objid, void* thisptr, void(*callback)(void*,NamedObject*));
    void OpenNet_GetMissingReplicas(void* db, void* thisptr, bool(*callback)(void*,const char*));
    void OpenNet_AddReplica(void* db, const char* blob, const unsigned char* id);
    size_t RSA_Encrypt(unsigned char* key, size_t keylen, unsigned char* data, size_t dlen, unsigned char* output);
    size_t RSA_decrypt(unsigned char* key, size_t keylen, unsigned char* data, size_t dlen);
    void gen_aes_key(unsigned char* key);
    size_t OpenNet_RSA_Encrypt(void* db,const char* thumbprint, unsigned char* data, size_t len, unsigned char* output);
    size_t OpenNet_RSA_Decrypt(void* db,const char* thumbprint, unsigned char* data, size_t len);
    bool OpenNet_HasPrivateKey(void* db,const char* thumbprint);
    bool GGDNS_Resolve(const char* dotname, const char* localKey, unsigned char* output);
    void OpenNet_ExportKey(void* db, const char* key,void* thisptr,void(*callback)(void*,unsigned char*,size_t));
    size_t OpenNet_ImportKey(void* db, const unsigned char* blob);
    void OpenNet_EnumNamedObjects(void* db,void* thisptr, bool(*callback)(void*,const char*,NamedObject*));
    void OpenNet_EnumCertificates(void* db, void* thisptr, bool(*callback)(void*,const char*));
    void GGDNS_Backup(void* thisptr,void(*writeDgate)(void*,unsigned char*,size_t));
    void GGDNS_RestoreBackup(unsigned char* bytes, size_t sz);
    //Sets the insertion handler to be called when a new NamedObject is added to the database.
    void GGDNS_SetInsertionHandler(void* thisptr,void(*callback)(void*,NamedObject*,const char*));
    size_t GGDNS_ObjectsSynchronizing(); //Gets the number of objects currently synchronizing with GGDNS partners.
    extern size_t OpenNet_replicaCount;
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
//OS-Specific routines
extern "C" {
	//Creates a 20-byte hash
	void* CreateHash();
	void UpdateHash(void* hash, const unsigned char* data, size_t sz);
	void FinalizeHash(void* hash, unsigned char* output);
    bool VerifySignature(unsigned char* data, size_t dlen, unsigned char* signature, size_t slen, unsigned char* key);
    size_t CreateSignature(const unsigned char* data, size_t dlen, unsigned char* privateKey, unsigned char* signature);
    bool isValidKey(unsigned char* data, size_t len, bool* isPrivate);
    unsigned char* CreatePrivateKey(size_t* len, size_t* pubLen);
    const char* GetKeyDbFileName();
}

class SafeBuffer {
public:
    void* ptr;
    size_t sz;
    int Read(unsigned char* buffer, int count) {
        if (pos + count > sz) {
            throw "up";
        }

        memcpy(buffer, ((unsigned char*)ptr) + pos, count);

        pos += count;
        return count;
    }
    void Write(unsigned char* data, int count) {
        if (pos + count > sz) {
            throw "up";
        }
        memcpy(((unsigned char*)ptr) + pos, data, count);

        pos += count;
    }
    size_t pos;
    int64_t GetLength() {
        return (int64_t)sz;
    }
    template<typename T>
    void Read(T& val) {
        Read((unsigned char*)&val, sizeof(val));
    }
    template<typename T>
    void Write(const T& val) {
        Write((unsigned char*)&val, sizeof(val));
    }
    SafeBuffer(void* ptr, size_t sz) {
        this->ptr = ptr;
        this->sz = sz;
        pos = 0;
    }
};

#endif
#endif







//GGDNS header file
#ifndef GGDNS_H
#define GGDNS_H
#ifdef __cplusplus
extern "C" {
#endif
//Initializes GGDNS
void GGDNS_Init(void* mngr);
void GGDNS_RunQuery(const char* name,void* thisptr, void(*callback)(void*,NamedObject*));
void GGDNS_EnumPrivateKeys(void* thisptr,bool(*enumCallback)(void*,const char*));
//Adds an object to the local database and attempts to replicate it to the desired number of replica
//servers asynchronously. The optional callback is invoked when replication completes or times out.
void GGDNS_MakeObject(const char* name, NamedObject* object, void* thisptr,  void(*callback)(void*,bool));
//Sets the desired number of replicas for this dataset. Changing this number does NOT
//effect data already in the database. This only changes the number of replicas
//data will be written to before reporting a successful write. Data that is not
//successfully replicated will still be added to your local database instance,
//and may also be cached on other database servers if those objects are requested.
void GGDNS_SetReplicaCount(size_t count);
void GGDNS_QueryDomain(const char* name, const char* parent, void* tptr, void(*callback)(void*,const char*));
void GGDNS_GetGuidListForObject(const char* objid,void* thisptr, void(*callback)(void*,unsigned char*,size_t));
void GGDNS_SetTimeoutInterval(size_t ms);
void GGDNS_MakeDomain(const char* name, const char* parent,  const char* authority,void* thisptr, void(*callback)(void* thisptr, unsigned char* data, size_t dlen));
//Modifies the host list for a domain name under your control (where ptr is the pointer to your domain)
//Length is the length (in bytes) of the guidlist. A guidlist should
//be composed of individual 16-byte entries.
void GGDNS_MakeHost(const char* ptr, unsigned char* guidlist, size_t len);
void* GGDNS_db();
bool GGDNS_ResolveHost(const char* authority, const char* name, unsigned char* output, unsigned char* key);
#ifdef __cplusplus
}
#endif


#ifdef __cplusplus
//TODO: C++ helpers
static std::string DotQuery(const char* query, bool* resolved) {
  *resolved = true;
	auto expect = [](const char*& str, const char& value, bool& found){
		if(str == 0) {
			return std::string("");
		}
		size_t offset = 0;
		found = false;
		while(*(str+offset) != 0) {
			if(*(str+offset) == value) {
				//TODO: Take substring
				str = str+offset+1;
				found = true;
				return std::string(str-offset-1,offset);
			}
			offset++;
		}
		return std::string(str);
	};
	std::vector<std::string> components;
	bool found = true;
	while(true) {
		std::string expected = expect(query,'.',found);
		components.push_back(expected);
		if(!found) {
			break;
		}
	}
	std::string parent;
	for(ssize_t i = components.size()-1;i>=0;i--) {
		Event m;
		void(*cb)(void*,const char*);
		bool abort = false;
		void* thisptr = C([&](const char* name){
			if(name != 0) {
				parent = name;
			}else {
			  *resolved = false; //error occured during name resolution.
			  abort = true;
			}
			m.signal();
		},cb);
		GGDNS_QueryDomain(components[i].data(),parent.data(),thisptr,cb);
		m.wait();
		if(abort) {
		  break;
		}
	}
	return parent;
}
#endif

#endif // GGDNS_H
