

#ifndef MESSAGE_VALIDATION_H
#define MESSAGE_VALIDATION_H

#include "server_config.h"

uint16_t compute_checksum(char* input , uint32_t len);
uint16_t validate_length(char* msg , uint16_t msg_len);
uint16_t validate_magic(char* msg);
uint16_t validate_checksum(char* msg , uint16_t msg_len);

#endif
