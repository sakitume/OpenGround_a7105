/*
    Copyright 2019 santipc <AT> gmail.com

    This program is free software: you can redistribute it and/ or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http:// www.gnu.org/licenses/>.

    author: santipc <AT> gmail.com
*/

#include "proto.h"
#include "a7105.h"
#include "frsky.h"
#include "debug.h"

void proto_init(void) {
    debug("proto: init\n"); debug_flush();

    frsky_init();
}

char *rf_get_proto_name(uint8_t i) {
    switch (i) {
        default                     : return "???";
        case (PROTO_AFHDS)          : return "AFHDS";
        case (PROTO_AFHDS2A)        : return "AFHDS2A";
        case (PROTO_HUBSAN)         : return "HUBSAN";
    }
}


