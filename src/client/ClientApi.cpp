#include "ClientApi.h"
#include "ClientApiImpl.h"

ClientApi* ClientApi::createClientApi() {

	ClientApi* client_api = new ClientApiImpl();

	return client_api;
}
