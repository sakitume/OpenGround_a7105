/*
    Copyright 2019 santipc <AT> gmail.com

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    author: santipc <AT> gmail.com
*/

#ifndef PROTO_H_
#define PROTO_H_
#include <stdbool.h>
#include <stdint.h>
#include "main.h"

void proto_init(void);

// Supported protocols
typedef enum {
  PROTO_AFHDS = 0,
  PROTO_AFHDS2A,
  PROTO_HUBSAN,
  PROTO_SIZE
} protocol_id_t;

char *rf_get_proto_name(uint8_t i);


#endif  // PROTO_H_
