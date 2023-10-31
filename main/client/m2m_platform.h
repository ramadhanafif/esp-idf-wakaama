

#include <liblwm2m.h>

lwm2m_object_t *get_object_device(void);
lwm2m_object_t *get_security_object(void);
lwm2m_object_t *get_server_object(void);
void output_buffer(const uint8_t *buffer, size_t length, int indent);
