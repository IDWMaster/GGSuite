//GLOBALGRID NATIVES
#include "GlobalGrid.h"
#include <uuid/uuid.h>
#include <unistd.h>
using namespace GlobalGrid;
ByteArray OS_ResolveByteArray(void* byteArray) {
	//Resolve a byte array
	ByteArray retval;
	SafeBuffer* buffer = (SafeBuffer*)byteArray;
	retval.data = buffer->data;
	retval.size = buffer->len;
	return retval;
}
void OS_GetSystemGuid(unsigned char* id) {
	//Generate a pseudo-random GUID and bind to this interface
	uuid_generate(id);
}
    void OS_Free(void* obj) {
    	delete ((IDisposable*)obj);
    }
    void OS_Sleep(int seconds) {
    	sleep(seconds);
    }
