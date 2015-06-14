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
#include <openssl/sha.h>
#include <openssl/rsa.h>
#include <openssl/objects.h>
#include <openssl/aes.h>
#include <openssl/rand.h>
static char filename[250];





void gen_aes_key(unsigned char* key) {
while(RAND_bytes(key,16) != 1) {}
}


const char* GetKeyDbFileName() {
  return "key.db";
}
void* CreateHash() {
    SHA_CTX* ctx = (SHA_CTX*)malloc(sizeof(SHA_CTX));
    SHA1_Init(ctx);
    return ctx;
}
void UpdateHash(void* hash, const unsigned char* data, size_t sz) {
    SHA_CTX* ctx = (SHA_CTX*)hash;
    SHA1_Update(ctx, data, sz);
}
void FinalizeHash(void* hash, unsigned char* output) {
    SHA1_Final(output, (SHA_CTX*)hash);
}
size_t CreateSignature(const unsigned char* data, size_t dlen, unsigned char* privateKey, unsigned char* signature);
bool isValidKey(unsigned char* data, size_t length, bool* isPrivate) {
    *isPrivate = false;
    uint32_t len;
    if(length<4) {
        return false;
    }
    memcpy(&len, data, 4);
    length-=4;
    data+=4;
    if (length<len) {
        return false;
    }
    data+=len;
    length-=len;
    if(length<4) {
        return false;
    }
    memcpy(&len, data, 4);
    length-=4;
    data+=4;
    if(length<len) {
    	return false;
    }
    length-=len;
    data+=len;
    if (length>=4) {
        *isPrivate = true;
        memcpy(&len, data, 4);
        length-=4;
        data+=4;
        if (length<len) {
            return false;
        }
        return true;
    }else {
        return true;
    }

}
unsigned char* CreatePrivateKey(size_t* len, size_t* pubLen) {
    //MOD, PUB_EXP, PRIV_EXP
    RSA* msa = RSA_new();
    BIGNUM* e = BN_new();
    BN_set_word(e, 65537);
    RSA_generate_key_ex(msa, 4096, e, 0);
    BN_free(e);
    size_t pubSize = 4+BN_num_bytes(msa->n)+4+BN_num_bytes(msa->e);
    size_t privSize = 4+BN_num_bytes(msa->d);
    unsigned char* retval = (unsigned char*)malloc(pubSize+privSize);
    unsigned char* izard = retval;
    uint32_t count = BN_num_bytes(msa->n);
    memcpy(izard, &count, 4);
    izard+=4;
    BN_bn2bin(msa->n, izard);
    izard+=count;
    count = BN_num_bytes(msa->e);
    memcpy(izard, &count, 4);
    izard+=4;
    BN_bn2bin(msa->e, izard);
    izard+=count;
    count = BN_num_bytes(msa->d);
    memcpy(izard, &count, 4);
    izard+=4;
    BN_bn2bin(msa->d, izard);
    *len = pubSize+privSize;
    *pubLen = pubSize;
    RSA_free(msa);
    return retval;
}
#define R(output) Read(str,output)
#define RI ReadBig(str,len)
template<typename T>
static void Read(unsigned char*& str, T& output) {
    memcpy(&output,str,sizeof(output));
    str+=sizeof(output);
}
static BIGNUM* ReadBig(unsigned char*& str,size_t len) {
    BIGNUM* retval = BN_bin2bn(str,(int)len,0);
    str+=len;
    return retval;
}


size_t CreateSignature(const unsigned char* data, size_t dlen, unsigned char* privateKey, unsigned char* signature) {
   unsigned char hash[SHA256_DIGEST_LENGTH];
   RSA* msa = RSA_new();
   if(msa->n) {
       throw "down";
   }
   unsigned char* str = (unsigned char*)privateKey;
   uint32_t len;
   R(len);
   msa->n = RI;
   R(len);
   msa->e = RI;
   R(len);
   msa->d = RI;
   bool m = false;
       if(signature == 0) {
       signature = new unsigned char[RSA_size(msa)];
       m = true;
       }
   SHA256(data,(int)dlen,hash);
   unsigned int siglen = 0;

   RSA_sign(NID_sha256,hash,SHA256_DIGEST_LENGTH,signature,&siglen,msa);


   if(!VerifySignature((unsigned char*)data,dlen,signature,siglen,privateKey)) {
       printf("iPuked\n");
       abort();
   }



   if(m) {
    delete[] signature;
   }
    RSA_free(msa);



    return siglen;
}


size_t RSA_Encrypt(unsigned char* key, size_t keylen, unsigned char* data, size_t dlen, unsigned char* output) {
	RSA* msa = RSA_new();
	unsigned char* str = (unsigned char*)key;
	    uint32_t len;
	    R(len);
	    msa->n = RI;
	    R(len);
	    msa->e = RI;
	    size_t retval = RSA_size(msa);

	    if(output == 0) {
	    	size_t retval = RSA_size(msa);
	    	RSA_free(msa);
	    	return retval;
	    }
	    RSA_public_encrypt(dlen,data,output,msa,RSA_PKCS1_PADDING);
	    RSA_free(msa);

	    return retval;
}
size_t RSA_decrypt(unsigned char* key, size_t keylen, unsigned char* data, size_t dlen) {
	   RSA* msa = RSA_new();
	   if(msa->n) {
	       throw "down";
	   }
	   unsigned char* str = (unsigned char*)key;
	   uint32_t len;
	   R(len);
	   msa->n = RI;
	   R(len);
	   msa->e = RI;
	   R(len);
	   msa->d = RI;
	   return RSA_private_decrypt(dlen,data,data,msa,RSA_PKCS1_PADDING);


}

void AES_Encrypt(unsigned char* key, unsigned char* data) {
  AES_KEY _key;
  AES_set_encrypt_key(key,256,&_key);
  AES_encrypt(data,data,&_key);
}
void AES_Decrypt(unsigned char* key, unsigned char* data) {
  AES_KEY _key;
  AES_set_decrypt_key(key,256,&_key);
  AES_decrypt(data,data,&_key);
}


bool VerifySignature(unsigned char* data, size_t dlen, unsigned char* signature, size_t slen, unsigned char* key) {
    RSA* msa = RSA_new();
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(data,(int)dlen,hash);
    unsigned char* str = (unsigned char*)key;
    uint32_t len;
    R(len);
    msa->n = RI;
    R(len);
    msa->e = RI;

    bool retval = RSA_verify(NID_sha256,hash,SHA256_DIGEST_LENGTH,signature,slen,msa);

    RSA_free(msa);
    return retval;
}
