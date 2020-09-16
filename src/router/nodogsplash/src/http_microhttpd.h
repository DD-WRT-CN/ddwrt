#ifndef NDS_MICROHTTPD_H
#define NDS_MICROHTTPD_H

#include <stdio.h>

struct MHD_Connection;

int libmicrohttpd_cb (void *cls,
					struct MHD_Connection *connection,
					const char *url,
					const char *method,
					const char *version,
					const char *upload_data, size_t *upload_data_size, void **ptr);


#endif // NDS_MICROHTTPD_H
