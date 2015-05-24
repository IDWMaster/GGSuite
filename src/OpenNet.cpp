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




#include "OpenAuth.h"
#include "sqlite3.h"
#include <string.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <mutex>
std::string string_to_hex(const unsigned char* input, size_t len)
{
	static const char* const lut = "0123456789ABCDEF";

	std::string output;
	output.reserve(2 * len);
	for (size_t i = 0; i < len; ++i)
	{
		const unsigned char c = input[i];
		output.push_back(lut[c >> 4]);
		output.push_back(lut[c & 15]);
	}
	return output;
}

std::string hex_to_string(const std::string& input)
{
	static const char* const lut = "0123456789ABCDEF";
	size_t len = input.length();
	if (len & 1) throw std::invalid_argument("odd length");

	std::string output;
	output.reserve(len / 2);
	for (size_t i = 0; i < len; i += 2)
	{
		char a = input[i];
		const char* p = std::lower_bound(lut, lut + 16, a);
		if (*p != a) throw std::invalid_argument("not a hex digit");

		char b = input[i + 1];
		const char* q = std::lower_bound(lut, lut + 16, b);
		if (*q != b) throw std::invalid_argument("not a hex digit");

		output.push_back(((p - lut) << 4) | (q - lut));
	}
	return output;
}




class IDisposable {
public:
	virtual ~IDisposable(){};
};
class SafeResizableBuffer {
public:
	//This is a car
	unsigned char* buffer;
	size_t sz;
	SafeResizableBuffer() {
		buffer = 0;
		position = 0;
	}
	template<typename T>
	void Write(const T& value) {
		Write(&value,sizeof(value));
	}
	template<typename T>
	int Read(T& value) {
		return Read(&value,sizeof(value));
	}
	size_t position;
	int Read(void* buffer, int count) {
		if (position + count > sz) {
			//BARFIZARD!
			throw "up";
		}
		memcpy(buffer, this->buffer + position, count);
		position += count;
		return count;
	}
	void Write(const void* buffer, int count) {
		if (position + count > sz) {
			size_t allocsz = std::max(position + count, sz * 2);
			unsigned char* izard = new unsigned char[allocsz];
			memcpy(izard,this->buffer,sz);
			delete[] this->buffer;
		}
		memcpy(this->buffer, buffer, count);
	}
	~SafeResizableBuffer() {
		if(buffer) {
		delete[] buffer;
		}
	}
};

class Certificate:public IDisposable {
public:
	std::vector<unsigned char> PublicKey;
	std::vector<unsigned char> PrivateKey;
    std::vector<unsigned char> Signature;
    std::string Authority;

};
class KeyDatabase:public IDisposable {
public:
	std::recursive_mutex mtx;
	sqlite3* db;
	sqlite3_stmt* command_addcert;
	sqlite3_stmt* command_addobject;
	sqlite3_stmt* command_addkey;
	sqlite3_stmt* command_findcert;
    sqlite3_stmt* command_getPrivateKeys;
    sqlite3_stmt* command_findObject;
    sqlite3_stmt* command_findPrivateKey;
    sqlite3_stmt* command_enumerateCertificates;
    sqlite3_stmt* command_updateObject;
    sqlite3_stmt* command_findDomain;
    sqlite3_stmt* command_findRootDomain;
    sqlite3_stmt* command_findReverseDomain;
    sqlite3_stmt* command_addDomain;
    sqlite3_stmt* command_addReplica;
    sqlite3_stmt* command_takedownBlob;
    sqlite3_stmt* command_retrieveDomainPtr;
    sqlite3_stmt* command_addDomainPtr;
    sqlite3_stmt* command_findReplicas;
    sqlite3_stmt* command_findMissingReplicas;
    sqlite3_stmt* command_enumObjects;
    void EnumerateCertificates(void* thisptr,bool(*callback)(void*,const char*)) {
        int status;
        while ((status = sqlite3_step(command_enumerateCertificates)) != SQLITE_DONE) {
            if(status == SQLITE_ROW) {
            if(!callback(thisptr,(const char*)sqlite3_column_text(command_enumerateCertificates, 0))) {
                break;
            }
            }
        }
        sqlite3_reset(command_enumerateCertificates);
        
    }
    std::string AddCertificate(Certificate* cert) {
		void* hash = CreateHash();
		UpdateHash(hash, cert->PublicKey.data(), cert->PublicKey.size());
		//Use Unsigned Charizard's thumbprint
		unsigned char izard[20];
		FinalizeHash(hash, izard);

		
		std::string thumbprint = string_to_hex(izard,20);

		sqlite3_bind_text(command_addcert, 1, thumbprint.data(), thumbprint.size(), 0);
		sqlite3_bind_blob(command_addcert, 2, cert->PublicKey.data(), cert->PublicKey.size(),0);
		sqlite3_bind_text(command_addcert, 3, cert->Authority.data(), cert->Authority.size(), 0);
        sqlite3_bind_blob(command_addcert, 4, cert->Signature.data(),cert->Signature.size(),0);

		//Insert ificate
		int val;
        while ((val = sqlite3_step(command_addcert)) != SQLITE_DONE){  };
		
		sqlite3_reset(command_addcert);
		if (val == SQLITE_DONE) {
            if(cert->PrivateKey.size()) {
                //Add private key to database

                unsigned char* data = cert->PrivateKey.data();
                sqlite3_bind_text(command_addkey,1,thumbprint.data(),thumbprint.size(),0);
                sqlite3_bind_blob(command_addkey,2,data,cert->PrivateKey.size(),0);
                val = 0;
                while((val = sqlite3_step(command_addkey)) != SQLITE_DONE){};
                sqlite3_reset(command_addkey);
                return thumbprint;

            }
            return thumbprint;
		}
		return "";
	}
	Certificate* FindCertificate(const std::string& thumbprint) {
		sqlite3_bind_text(command_findcert, 1, thumbprint.data(), thumbprint.size(), 0);
		int val;
		Certificate* retval = 0;
		while ((val = sqlite3_step(command_findcert)) != SQLITE_DONE) {
			if (val == SQLITE_ROW) {
				retval = new Certificate();
				retval->PublicKey.resize(sqlite3_column_bytes(command_findcert, 1));
				memcpy(retval->PublicKey.data(), sqlite3_column_blob(command_findcert, 1),retval->PublicKey.size());
				retval->Authority = (const char*)sqlite3_column_text(command_findcert, 2);
                retval->Signature.resize(sqlite3_column_bytes(command_findcert,3));
                memcpy(retval->Signature.data(),sqlite3_column_blob(command_findcert,3),sqlite3_column_bytes(command_findcert,3));
                break;
			}
		}
        sqlite3_reset(command_findcert);
        sqlite3_bind_text(command_findPrivateKey,1,thumbprint.data(),thumbprint.size(),0);
        if(retval) {
            //Locate private key
            while((val = sqlite3_step(command_findPrivateKey)) != SQLITE_DONE) {
                if(val == SQLITE_ROW) {
                const void* data = sqlite3_column_blob(command_findPrivateKey,1);
                size_t sz = sqlite3_column_bytes(command_findPrivateKey,1);
                retval->PrivateKey.resize(sz);
                memcpy(retval->PrivateKey.data(),data,sz);
                break;
                }
            }
            sqlite3_reset(command_findPrivateKey);
        }
        return retval;
	}
    void EnumeratePrivateKeys(bool(*callback)(const char* thumbprint)) {
        int val;
        while((val = sqlite3_step(command_getPrivateKeys)) != SQLITE_DONE) {
            if(val == SQLITE_ROW) {
                if(!callback((const char*)sqlite3_column_text(command_getPrivateKeys,0))) {
                    break;
                }
            }
        }
        sqlite3_reset(command_getPrivateKeys);
    }

	KeyDatabase() {
		sqlite3_open(GetKeyDbFileName(), &db);
		char* err;
		//The thumbprint is a hash of the public key
		//Certificates (though not necessarily TRUSTED certificates)
        sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS Certificates (Thumbprint TEXT, PublicKey BLOB, Authority TEXT, Signature BLOB, PRIMARY KEY(Thumbprint))",0,0,&err);
        //Raw object repository
        sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS NamedObjects (Name TEXT, Authority TEXT, Signature BLOB, SignedData BLOB)", 0, 0, &err);
        //Private keys
        sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS PrivateKeys (Thumbprint TEXT PRIMARY KEY, PrivateKey BLOB)",0,0,&err);
        //Domain metadata table (for fast DNS lookup)
        //Name = the name of the domain relative to the name of the GUID in Parent
        //(for root nodes; this value can be NULL)
        //Parent = The GUID of the parent node.
        //ObjectID = The actual signed DNS object, validating the identity of the domain.
        sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS Domains (Name TEXT NOT NULL, Parent TEXT, ObjectID TEXT NOT NULL, PRIMARY KEY(Name, Parent)); CREATE INDEX IF NOT EXISTS ReverseLookup ON Domains(ObjectID)",0,0,&err);
        sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS DomainPointers (DomainID TEXT NOT NULL PRIMARY KEY, ObjectID TEXT NOT NULL)",0,0,&err);

        //Replica server list (per-object)
        sqlite3_exec(db,"CREATE TABLE IF NOT EXISTS Replicas (ObjectName TEXT NOT NULL PRIMARY KEY, ServerID TEXT, replicaCount INTEGER)",0,0,&err);
        std::string sql = "INSERT OR IGNORE INTO Certificates VALUES (?, ?, ?, ?)";
		const char* parsed;
		sqlite3_prepare(db, sql.data(), (int)sql.size(), &command_addcert, &parsed);
        sql = "INSERT INTO NamedObjects VALUES (?, ?, ?, ?)";
		sqlite3_prepare(db, sql.data(), (int)sql.size(), &command_addobject, &parsed);
		sql = "INSERT INTO PrivateKeys VALUES (?, ?)";
		sqlite3_prepare(db, sql.data(), (int)sql.size(), &command_addkey, &parsed);
        sql = "SELECT * FROM Certificates WHERE Thumbprint = ?";
        sqlite3_prepare(db, sql.data(), (int)sql.size(), &command_findcert, &parsed);
        sql = "SELECT * FROM NamedObjects WHERE Name = ?";
        sqlite3_prepare(db, sql.data(), (int)sql.size(), &command_findObject, &parsed);
        sql = "SELECT * FROM PrivateKeys WHERE Thumbprint = ?";
        sqlite3_prepare(db, sql.data(), (int)sql.size(), &command_findPrivateKey, &parsed);
        sql = "SELECT * FROM PrivateKeys";
        sqlite3_prepare(db,sql.data(),(int)sql.size(),&command_getPrivateKeys,&parsed);
        sql = "SELECT * FROM Certificates";
        sqlite3_prepare(db,sql.data(),(int)sql.size(),&command_enumerateCertificates,&parsed);
        sql = "UPDATE NamedObjects SET Authority = ?, Signature = ?, SignedData = ? WHERE Name = ?;DELETE FROM Replicas WHERE ObjectName = ?";
        sqlite3_prepare(db,sql.data(),(int)sql.size(),&command_updateObject,&parsed);
        sql = "SELECT ObjectID FROM Domains WHERE Name = ? AND Parent = ?";
        sqlite3_prepare(db,sql.data(),(int)sql.size(),&command_findDomain,&parsed);
        sql = "SELECT ObjectID FROM Domains WHERE Name = ? AND Parent IS NULL";
        sqlite3_prepare(db,sql.data(),(int)sql.size(),&command_findRootDomain,&parsed);
        sql = "SELECT Name, Parent FROM Domains WHERE ObjectID = ?";
        sqlite3_prepare(db,sql.data(),(int)sql.size(),&command_findReverseDomain,&parsed);
        sql = "INSERT OR IGNORE INTO Domains VALUES (?, ?, ?)";
        sqlite3_prepare(db,sql.data(),(int)sql.size(),&command_addDomain,&parsed);
        sql = "INSERT OR REPLACE INTO Replicas VALUES (?1, ?2, ?3);";
        sqlite3_prepare(db,sql.data(),(int)sql.size(),&command_addReplica,&parsed);
        sql = "DELETE FROM NamedObjects WHERE Name = ?";
        sqlite3_prepare(db,sql.data(),(int)sql.size(),&command_takedownBlob,&parsed);
        sql = "SELECT * FROM DomainPointers WHERE DomainID = ?";
        sqlite3_prepare(db,sql.data(),(int)sql.size(),&command_retrieveDomainPtr,&parsed);
        sql = "INSERT INTO DomainPointers VALUES (?, ?)";
        sqlite3_prepare(db,sql.data(),(int)sql.size(),&command_addDomainPtr,&parsed);
        sql = "SELECT ServerID FROM Replicas WHERE ObjectName = ?";
        sqlite3_prepare(db,sql.data(),(int)sql.size(),&command_findReplicas,&parsed);
        sql = "SELECT ObjectName FROM Replicas WHERE ReplicaCount < ?";
        sqlite3_prepare(db,sql.data(),(int)sql.size(),&command_findMissingReplicas,&parsed);
        sql = "SELECT * FROM NamedObjects";
        sqlite3_prepare(db,sql.data(),(int)sql.size(),&command_enumObjects,&parsed);

        int status = 0;
        bool hasKey = false;
        while((status = sqlite3_step(command_getPrivateKeys)) != SQLITE_DONE) {
            if(status == SQLITE_ROW) {
            hasKey = true;
            }

        }
        if(!hasKey) {
            //Create a new key
            Certificate cert;
            size_t keylen;
            size_t publen;
            unsigned char* key = CreatePrivateKey(&keylen,&publen);
            cert.PrivateKey.resize(keylen);
            memcpy(cert.PrivateKey.data(),key,keylen);
            cert.PublicKey.resize(publen);
            memcpy(cert.PublicKey.data(),key,publen);
            bool p;
            if(!isValidKey(cert.PublicKey.data(),cert.PublicKey.size(),&p)) {
            	throw "up";
            }
            if(AddCertificate(&cert).size() == 0) {
                throw "sideways";
            }
            free(key);
        }

	}
    bool AddObject(const NamedObject& obj, const char* name, bool update = false) {
        //Adds a named object to the database
        //Verify signature on BOTH name and data
        bool foundAuthority = false;
        Certificate* cert = FindCertificate(obj.authority);
        if(!cert) {
            return false;
        }
        size_t slen = strlen(name);
        unsigned char* mander = new unsigned char[slen+obj.bloblen];
        memcpy(mander,name,slen);
        memcpy(mander+slen,obj.blob,obj.bloblen);
        bool retval = VerifySignature(mander,slen+obj.bloblen,obj.signature,obj.siglen,cert->PublicKey.data());
        delete[] mander;
        if(retval) {
        	if(update) {
        		//UPDATE NamedObjects SET Authority = ?, Signature = ?, SignedData = ? WHERE Name = ?
        		sqlite3_bind_text(command_updateObject,1,obj.authority,strlen(obj.authority),0);
        		sqlite3_bind_blob(command_updateObject,2,obj.signature,obj.siglen,0);
        		sqlite3_bind_blob(command_updateObject,3,obj.blob,obj.bloblen,0);
        		sqlite3_bind_text(command_updateObject,4,name,slen,0);
        		sqlite3_bind_text(command_updateObject,5,obj.authority,strlen(obj.authority),0);
        		while(sqlite3_step(command_updateObject) != SQLITE_DONE) {}
        		sqlite3_reset(command_updateObject);
        	}else {
            //Name TEXT, Authority TEXT, Signature BLOB, SignedData BLOB
            sqlite3_bind_text(command_addobject,1,name,slen,0);
            sqlite3_bind_text(command_addobject,2,obj.authority,strlen(obj.authority),0);
            sqlite3_bind_blob(command_addobject,3,obj.signature,obj.siglen,0);
            sqlite3_bind_blob(command_addobject,4,obj.blob,obj.bloblen,0);
            while(sqlite3_step(command_addobject) != SQLITE_DONE) {}
            sqlite3_reset(command_addobject);
        	}
        }else {

        }
        return retval;
    }
    void RetrieveObject(const char* name, void* thisptr, void(*callback)(void*, NamedObject* val)) {

        NamedObject output;
        sqlite3_bind_text(command_findObject,1,name,strlen(name),0);
        int val;
        while((val = sqlite3_step(command_findObject)) != SQLITE_DONE) {
            if(val == SQLITE_ROW) {
                output.authority = (char*)sqlite3_column_text(command_findObject,1);
                output.signature = (unsigned char*)sqlite3_column_blob(command_findObject,2);
                output.siglen = sqlite3_column_bytes(command_findObject,2);
                output.blob = (unsigned char*)sqlite3_column_blob(command_findObject,3);
                output.bloblen = sqlite3_column_bytes(command_findObject,3);
                callback(thisptr,&output);
                break;
            }
        }
        sqlite3_reset(command_findObject);
    }
	~KeyDatabase() {
		sqlite3_finalize(command_addcert);
        sqlite3_finalize(command_addkey);
        sqlite3_finalize(command_addobject);
        sqlite3_finalize(command_findcert);
        sqlite3_finalize(command_findObject);
        sqlite3_finalize(command_findPrivateKey);
        sqlite3_finalize(command_getPrivateKeys);
        sqlite3_finalize(command_enumerateCertificates);
        sqlite3_finalize(command_findDomain);
        sqlite3_finalize(command_findReverseDomain);
        sqlite3_finalize(command_addDomain);
        sqlite3_finalize(command_addReplica);
        sqlite3_finalize(command_takedownBlob);
        sqlite3_finalize(command_retrieveDomainPtr);
        sqlite3_finalize(command_addDomainPtr);
        sqlite3_finalize(command_findRootDomain);
        sqlite3_finalize(command_findMissingReplicas);
        sqlite3_finalize(command_findReplicas);
        sqlite3_finalize(command_enumObjects);
		sqlite3_close(db);
	}
};
extern "C" {

bool OpenNet_HasPrivateKey(void* db,const char* thumbprint) {
	KeyDatabase* realdb = (KeyDatabase*)db;
	std::unique_lock<std::recursive_mutex> l(realdb->mtx);
	Certificate* cert = realdb->FindCertificate(thumbprint);
	if(cert == 0) {
		return false;
	}
	bool retval = cert->PrivateKey.size()>0;
	delete cert;
	return retval;
}

size_t OpenNet_RSA_Encrypt(void* db,const char* thumbprint, unsigned char* data, size_t len, unsigned char* output) {

	KeyDatabase* realdb = (KeyDatabase*)db;

	std::unique_lock<std::recursive_mutex> l(realdb->mtx);
	Certificate* cert = realdb->FindCertificate(thumbprint);
	size_t retval = RSA_Encrypt(cert->PublicKey.data(),cert->PublicKey.size(),data,len,output);
	delete cert;
	return retval;
}
size_t OpenNet_RSA_Decrypt(void* db,const char* thumbprint, unsigned char* data, size_t len) {
	KeyDatabase* realdb = (KeyDatabase*)db;

	std::unique_lock<std::recursive_mutex> l(realdb->mtx);
	Certificate* cert = realdb->FindCertificate(thumbprint);
	size_t retval = RSA_decrypt(cert->PrivateKey.data(),cert->PrivateKey.size(),data,len);
	delete cert;
	return retval;

}

	void OpenNet_OAuthAddReplicaServer(void* db, const char* object, const char* server) {
		KeyDatabase* realdb = (KeyDatabase*)db;

		std::unique_lock<std::recursive_mutex> l(realdb->mtx);
		//TODO: Finish
	}

    void OpenNet_OAuthEnumPrivateKeys(void* db, void* thisptr, bool(*callback)(void* thisptr,const char* thumbprint)) {
        KeyDatabase* realdb = (KeyDatabase*)db;

    	std::unique_lock<std::recursive_mutex> l(realdb->mtx);
        int status;
        while((status = sqlite3_step(realdb->command_getPrivateKeys)) != SQLITE_DONE) {
            if(status == SQLITE_ROW) {
                if(!callback(thisptr,(char*)sqlite3_column_text(realdb->command_getPrivateKeys,0))) {
                    break;
                }
            }
        }
        sqlite3_reset(realdb->command_getPrivateKeys);

    }
    void OpenNet_EnumCertificates(void* db, void* thisptr, bool(*callback)(void*,const char*)) {
    	KeyDatabase* realdb = (KeyDatabase*)db;
    	std::unique_lock<std::recursive_mutex> l(realdb->mtx);
    	realdb->EnumerateCertificates(thisptr,callback);
    }
    void OpenNet_AddDomainPtr(void* db, const char* objid, const char* ptrObject) {
    	KeyDatabase* keydb = (KeyDatabase*)db;

    	std::unique_lock<std::recursive_mutex> l(keydb->mtx);
    	sqlite3_stmt* query = keydb->command_addDomainPtr;
    	sqlite3_bind_text(query,1,objid,strlen(objid),0);
    	sqlite3_bind_text(query,2,ptrObject,strlen(ptrObject),0);
    	int val;
    	while((val = sqlite3_step(query)) != SQLITE_DONE) {};

    }
    void OpenNet_RetrieveDomainPtr(void* db, const char* objid, void* thisptr, void(*callback)(void*,NamedObject*)) {
    	KeyDatabase* keydb = (KeyDatabase*)db;
    	std::unique_lock<std::recursive_mutex> l(keydb->mtx);
    	sqlite3_stmt* query = keydb->command_retrieveDomainPtr;
    	int val;
    	std::string name;
    	sqlite3_bind_text(query,1,objid,strlen(objid),0);
    	while((val = sqlite3_step(query)) != SQLITE_DONE) {
    		if(val == SQLITE_ROW) {
    			name = (const char*)sqlite3_column_text(query,1);
    		}
    	}
    	sqlite3_reset(query);
    	if(name.size()) {
    		OpenNet_Retrieve(db,name.data(),thisptr,callback);
    	}

    }
    void OpenNet_FindDomain(void* db, const char* domain, const char* parent, void* thisptr, void(*callback)(void*, const char*)) {
    	KeyDatabase* keydb = (KeyDatabase*)db;
    	std::unique_lock<std::recursive_mutex> l(keydb->mtx);
    	sqlite3_stmt* query = keydb->command_findDomain;
    	if(strlen(parent)) {
    	  	sqlite3_bind_text(query,2,parent,strlen(parent),0);
      	}else {
      		query = keydb->command_findRootDomain;
      	}
    	sqlite3_bind_text(query,1,domain,strlen(domain),0);

    	int val;
    	while((val = sqlite3_step(query)) != SQLITE_DONE) {
    		if(val == SQLITE_ROW) {
    			callback(thisptr,(char*)sqlite3_column_text(query,0));
    		}
    	}
    	sqlite3_reset(query);

    }
    void OpenNet_FindReverseDomain(void* db, const char* objid, void* thisptr, void(*callback)(void*,const char*, const char*)) {
    	KeyDatabase* keydb = (KeyDatabase*)db;
    	std::unique_lock<std::recursive_mutex> l(keydb->mtx);
    	auto bot = keydb->command_findReverseDomain;
    	sqlite3_bind_text(bot,1,objid,strlen(objid),0);
    	int val;
    	while((val = sqlite3_step(bot)) != SQLITE_DONE) {
    		if(val == SQLITE_ROW) {
    			callback(thisptr,(char*)sqlite3_column_text(bot,0),(char*)sqlite3_column_text(bot,1));
    		}
    	}
    	sqlite3_reset(bot);
    }

    void OpenNet_AddDomain(void* db, const char* name, const char* parent, const char* objid) {
    	KeyDatabase* keydb = (KeyDatabase*)db;
    	std::unique_lock<std::recursive_mutex> l(keydb->mtx);
    	auto bot = keydb->command_addDomain;
    	sqlite3_bind_text(bot,1,name,strlen(name),0);
    	if(parent) {
    	sqlite3_bind_text(bot,2,parent,strlen(parent),0);
    	}else{
    		sqlite3_bind_null(bot,2);
    	}
    	sqlite3_bind_text(bot,3,objid,strlen(objid),0);
    	while(sqlite3_step(bot) != SQLITE_DONE){};
    	sqlite3_reset(bot);
    }
    void* OpenNet_OAuthInitialize() {
		return new KeyDatabase();
    }
    void OpenNet_OAuthDestroy(void* db) {
        delete (KeyDatabase*)db;
    }
    //Finds the trust anchor for a given certificate; as well as an indication of the resolution status
    void OpenNet_ResolveChain(void* db, const char* thumbprint, void* thisptr, void(*callback)(void*, const char*, bool)) {
    	KeyDatabase* keydb = (KeyDatabase*)db;
    	std::unique_lock<std::recursive_mutex> l(keydb->mtx);
    	std::string current = thumbprint;
    	while(true) {

        	Certificate* cert = keydb->FindCertificate(current);
        	if(cert) {
        		if(cert->Authority.size() == 0) {
        			//We've reached the end of the chain.
        			callback(thisptr,current.data(),true);
        			delete cert;
        			break;
        		}else {
        			//Move on to the next one in the chain
        			Certificate* prev = cert;
        			current = prev->Authority;
        			delete prev;
        		}
        	}else {
        		//We've lost the chain (need to resolve missing link).
        		callback(thisptr,current.data(),false);
        		break;
        	}
    	}
    }
    void OpenNet_MakeObject(void* db, const char* name,  NamedObject* obj, bool update) {
        KeyDatabase* realdb = (KeyDatabase*)db;
        std::unique_lock<std::recursive_mutex> l(realdb->mtx);
        int status;
        sqlite3_bind_text(realdb->command_findPrivateKey,1,obj->authority,strlen(obj->authority),0);
        while((status = sqlite3_step(realdb->command_findPrivateKey)) != SQLITE_DONE) {
            if(status == SQLITE_ROW) {
                //Found key!
                unsigned char* privKey = (unsigned char*)sqlite3_column_blob(realdb->command_findPrivateKey,1);
                size_t privLen = sqlite3_column_bytes(realdb->command_findPrivateKey,1);
                //Sign blob using private key
                size_t signedBlobLen = strlen(name)+obj->bloblen;
                unsigned char* signedBlob = (unsigned char*)malloc(signedBlobLen);
                memcpy(signedBlob,name,strlen(name));
                memcpy(signedBlob+strlen(name),obj->blob,obj->bloblen);
                size_t siglen = CreateSignature(signedBlob,signedBlobLen,privKey,0);
                 //Insert into database
                obj->signature = (unsigned char*)malloc(siglen);
                obj->siglen = CreateSignature(signedBlob,signedBlobLen,privKey,obj->signature);
                if(update) {
                	OpenNet_UpdateObject(db,name,obj);
                }else {
                OpenNet_AddObject(db,name,obj);
                }
                free(signedBlob);
                free(obj->signature);
                break;
            }
        }
        sqlite3_reset(realdb->command_findPrivateKey);

        //TODO: Mark as needing replica
        sqlite3_stmt* query = realdb->command_addReplica;
        sqlite3_bind_text(query,1,name,strlen(name),0);
        sqlite3_bind_null(query,2);
        sqlite3_bind_int(query,3,0);
        while(sqlite3_step(query) != SQLITE_DONE) {}
        sqlite3_reset(query);
    }

    bool OpenNet_AddObject(void* db, const char* name, const NamedObject* obj) {
        KeyDatabase* keydb = (KeyDatabase*)db;
        std::unique_lock<std::recursive_mutex> l(keydb->mtx);
        return keydb->AddObject(*obj,name);
        
    }
    bool OpenNet_UpdateObject(void* db, const char* name, const NamedObject* obj) {
            KeyDatabase* keydb = (KeyDatabase*)db;
            std::unique_lock<std::recursive_mutex> l(keydb->mtx);
            return keydb->AddObject(*obj,name,true);

    }
    void OpenNet_RetrieveCertificate(void* db, const char* thumbprint,  void* thisptr,  void(*callback)(void*,OCertificate*)) {
    	KeyDatabase* keydb = (KeyDatabase*)db;
    	std::unique_lock<std::recursive_mutex> l(keydb->mtx);
    	Certificate* cert = keydb->FindCertificate(thumbprint);
    	if(cert) {
    		OCertificate abi;
    		abi.authority = (char*)cert->Authority.data();
    		abi.pubLen = cert->PublicKey.size();
    		abi.pubkey = (unsigned char*)cert->PublicKey.data();
    		abi.siglen = cert->Signature.size();
    		abi.signature = (unsigned char*)cert->Signature.data();
    		callback(thisptr,&abi);
    		delete cert;
    	}else {
    		callback(thisptr,0);
    	}
    }
    bool OpenNet_VerifySignature(void* db, const char* authority, unsigned char* data, size_t sz, unsigned char* signature, size_t siglen) {
    	KeyDatabase* keydb = (KeyDatabase*)db;
    	std::unique_lock<std::recursive_mutex> l(keydb->mtx);
    	Certificate* cert = keydb->FindCertificate(authority);
    	if(cert) {
    		bool retval = VerifySignature(data,sz,signature,siglen,cert->PublicKey.data());
    		delete cert;
    		return retval;
    	}
    	return false;
    }
    void OpenNet_SignData(void* db, const char* authority, unsigned char* data, size_t sz, void* thisptr, void(*callback)(void*,unsigned char*,size_t)) {
    	KeyDatabase* keydb = (KeyDatabase*)db;
    	std::unique_lock<std::recursive_mutex> l(keydb->mtx);
    	Certificate* cert = keydb->FindCertificate(authority);
    	if(cert) {
    		std::vector<unsigned char> pkey = cert->PrivateKey;
    		if(pkey.size() == 0) {
    			throw "sideways";
    		}
    		size_t slen = CreateSignature(data,sz,pkey.data(),0);
    		unsigned char* sig = new unsigned char[slen];
    		callback(thisptr,sig,slen);
    		callback(thisptr,sig,CreateSignature(data,sz,cert->PrivateKey.data(),sig));
    		delete[] sig;
    		delete cert;
    	}
    }
    void OpenNet_AddCertificate(void* db,const OCertificate* abi, void* thisptr, void(*callback)(void*,const char*)) {
    	Certificate cert;
    	cert.Authority = abi->authority;
    	cert.PublicKey.resize(abi->pubLen);
    	cert.Signature.resize(abi->siglen);
    	memcpy(cert.PublicKey.data(),abi->pubkey,abi->pubLen);
    	memcpy(cert.Signature.data(),abi->signature,abi->siglen);
    	KeyDatabase* keydb = (KeyDatabase*)db;
    	std::unique_lock<std::recursive_mutex> l(keydb->mtx);
    	bool priv;
    	if(isValidKey(abi->pubkey,abi->pubLen,&priv)) {
    		if(priv) {
    			if(callback) {
    				callback(thisptr,0);
    			}
    			return;
    		}
    		std::string retval = keydb->AddCertificate(&cert);
    		if(callback) {
    			callback(thisptr,retval.data());
    		}
    	}else {
    		if(callback) {
    			callback(thisptr,0);
    		}
    	}
    }
    void DMCA_TakedownBlob(void* db,const char* name) {
    	KeyDatabase* keydb = (KeyDatabase*)db;
    	std::unique_lock<std::recursive_mutex> l(keydb->mtx);
    	while(sqlite3_step(keydb->command_takedownBlob) != SQLITE_DONE) {}
    	sqlite3_reset(keydb->command_takedownBlob);
    }
    void OpenNet_Retrieve(void* db, const char* name, void* thisptr, void(*callback)(void *, NamedObject *)) {
        KeyDatabase* keydb = (KeyDatabase*)db;
        std::unique_lock<std::recursive_mutex> l(keydb->mtx);
        keydb->RetrieveObject(name,thisptr,callback);
    }
    void OpenNet_BeginTransaction(void* db) {
    	KeyDatabase* keydb = (KeyDatabase*)db;
    	std::unique_lock<std::recursive_mutex> l(keydb->mtx);
    	char* err;
    	sqlite3_exec(keydb->db, "BEGIN TRANSACTION",0,0,&err);

    }
    void OpenNet_EndTransaction(void* db) {
    	KeyDatabase* keydb = (KeyDatabase*)db;
    	std::unique_lock<std::recursive_mutex> l(keydb->mtx);
    	char* err;
    	sqlite3_exec(keydb->db, "COMMIT TRANSACTION",0,0,&err);

    }

    void OpenNet_GetMissingReplicas(void* db, void* thisptr, bool(*callback)(void*,const char*)) {
    	KeyDatabase* keydb = (KeyDatabase*)db;
    	std::unique_lock<std::recursive_mutex> l(keydb->mtx);
    	sqlite3_stmt* apocalypse = keydb->command_findMissingReplicas;
    	sqlite3_bind_int(apocalypse,1,(int)OpenNet_replicaCount);
    	while(sqlite3_step(apocalypse) != SQLITE_DONE) {
    		if(callback(thisptr,(const char*)sqlite3_column_text(apocalypse,0)) == 0) {
    			break;
    		}
    	}
    	sqlite3_reset(apocalypse);
    }
    //Export a private key
    void OpenNet_ExportKey(void* db, const char* key,void* thisptr,void(*callback)(void*,unsigned char*,size_t)) {
    	KeyDatabase* keydb = (KeyDatabase*)db;
    	std::unique_lock<std::recursive_mutex> l(keydb->mtx);
    	Certificate* cert = keydb->FindCertificate(key);
    	unsigned char* ms = new unsigned char[4+cert->PublicKey.size()+4+cert->PrivateKey.size()];
    	uint32_t len = (uint32_t)cert->PublicKey.size();
    	memcpy(ms,&len,4);
    	memcpy(ms+4,cert->PublicKey.data(),cert->PublicKey.size());
    	len = (uint32_t)cert->PrivateKey.size();
    	memcpy(ms+4+cert->PublicKey.size(),&len,4);
    	memcpy(ms+4+cert->PublicKey.size()+4,cert->PrivateKey.data(),len);
    	callback(thisptr,ms,4+cert->PublicKey.size()+4+cert->PrivateKey.size());
    	delete[] ms;
    	delete cert;
    }
    size_t OpenNet_ImportKey(void* db, const unsigned char* blob) {
    	size_t orig = (size_t)blob;
    	KeyDatabase* keydb = (KeyDatabase*)db;
    	std::unique_lock<std::recursive_mutex> l(keydb->mtx);
    	Certificate cert;
    	uint32_t sz = 0;
    	memcpy(&sz,blob,4);
    	cert.PublicKey.resize(sz);
    	blob+=4;
    	memcpy(cert.PublicKey.data(),blob,sz);
    	blob+=sz;
    	memcpy(&sz,blob,4);
    	blob+=4;
    	cert.PrivateKey.resize(sz);
    	memcpy(cert.PrivateKey.data(),blob,sz);
    	blob+=sz;
    	keydb->AddCertificate(&cert);
    	return (size_t)blob-orig;
    }
    void OpenNet_EnumNamedObjects(void* db,void* thisptr, bool(*callback)(void*,const char*,NamedObject*)) {
    	KeyDatabase* keydb = (KeyDatabase*)db;
    	std::unique_lock<std::recursive_mutex> l(keydb->mtx);
    	int val;
    	while((val = sqlite3_step(keydb->command_enumObjects)) != SQLITE_DONE) {
    		if(val == SQLITE_ROW) {
    		NamedObject obj;
    		const char* name = (char*)sqlite3_column_text(keydb->command_enumObjects,0);
    		obj.authority = (char*)sqlite3_column_text(keydb->command_enumObjects,1);
    		obj.signature = (unsigned char*)sqlite3_column_blob(keydb->command_enumObjects,2);
    		obj.siglen = sqlite3_column_bytes(keydb->command_enumObjects,2);
    		obj.blob = (unsigned char*)sqlite3_column_blob(keydb->command_enumObjects,3);
    		obj.bloblen = sqlite3_column_bytes(keydb->command_enumObjects,3);
    		if(!callback(thisptr,name,&obj)) {
    			break;
    		}
    		}
    	}
    	sqlite3_reset(keydb->command_enumObjects);

    }


    void OpenNet_AddReplica(void* db, const char* blob, const unsigned char* id) {
    	KeyDatabase* keydb = (KeyDatabase*)db;
    	std::unique_lock<std::recursive_mutex> l(keydb->mtx);
    	sqlite3_stmt* query = keydb->command_findReplicas;
    	sqlite3_bind_text(query,1,blob,strlen(blob),0);
    	int val;
    	bool hasValue = false;
    	while((val = sqlite3_step(query)) != SQLITE_DONE) {
    		if(val == SQLITE_ROW) {
    			hasValue = true;
    			break;
    		}
    	}

    	unsigned char* newval;
    	size_t newlen;
    	if(hasValue) {
    		unsigned char* prev = (unsigned char*)sqlite3_column_blob(query,0);
    		size_t prevlen = sqlite3_column_bytes(query,0);
    		if(prevlen>=16) {
    		for(size_t i = 0;i<prevlen;i+=16) {
    			if(memcmp(id,prev+i,16) == 0) {
    				//Invalid
    				sqlite3_reset(query);
    				return;
    			}
    		}
    		}
    		newval = new unsigned char[prevlen+16];
    		newlen = prevlen+16;
    		memcpy(newval,prev,prevlen);

    	}else {
    		newval = new unsigned char[16];
    		newlen = 16;
    	}
    	sqlite3_reset(query);
    	query = keydb->command_addReplica;
    	//objname,servers,length (number of servers)
    	sqlite3_bind_text(query,1,blob,strlen(blob),0);
    	sqlite3_bind_blob(query,2,newval,newlen,0);
    	sqlite3_bind_int(query,3,newlen/16);
    	while(sqlite3_step(query) !=SQLITE_DONE){};
    	sqlite3_reset(query);
    	delete[] newval;

    }
    size_t OpenNet_replicaCount;
}
