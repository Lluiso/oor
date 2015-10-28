/*
 *
 * Copyright (C) 2011, 2015 Cisco Systems, Inc.
 * Copyright (C) 2015 CBA research group, Technical University of Catalonia.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <netinet/in.h>
#include "lmlog.h"
#include "prefixes.h"

static inline lisp_addr_t * pref_get_network_address_v4(lisp_addr_t *address);

static inline lisp_addr_t * pref_get_network_address_v6(lisp_addr_t *address);

/*
 * If prefix b is contained in prefix a, then return TRUE. Otherwise return FALSE.
 * If both prefixs are the same it also returns TRUE
 */
int
pref_is_prefix_b_part_of_a (lisp_addr_t *a_prefix,lisp_addr_t *b_prefix)
{
    lisp_addr_t * a_network_addr;
    lisp_addr_t * b_network_addr_prefix_a;
    int a_pref_len;
    int b_pref_len;


    if (!lisp_addr_is_ip_pref(a_prefix) || !lisp_addr_is_ip_pref(b_prefix)){
        return FALSE;
    }

    if (lisp_addr_ip_afi(a_prefix) != lisp_addr_ip_afi(b_prefix)){
        return FALSE;
    }

    a_pref_len = lisp_addr_get_plen(a_prefix);
    b_pref_len = lisp_addr_get_plen(b_prefix);

    if (a_pref_len > b_pref_len){
        return FALSE;
    }

    a_network_addr = pref_get_network_address(a_prefix);

    lisp_addr_set_plen(b_prefix, a_pref_len);

    b_network_addr_prefix_a = pref_get_network_address(b_prefix);

    lisp_addr_set_plen(b_prefix, b_pref_len);

    if (lisp_addr_cmp (a_network_addr, b_network_addr_prefix_a) == 0){
        return (TRUE);
    }else{
        return (FALSE);
    }
}

lisp_addr_t *
pref_get_network_address(lisp_addr_t *address)
{
    lisp_addr_t *network_address = NULL;

    if (!lisp_addr_is_ip_pref(address)){
        LMLOG(LDBG_2, "get_network_address: Address %s is not a prefix ", lisp_addr_to_char(address));
        return (NULL);
    }

    switch (lisp_addr_ip_afi(address)){
    case AF_INET:
        network_address = pref_get_network_address_v4(address);
        break;
    case AF_INET6:
        network_address = pref_get_network_address_v6(address);
        break;
    default:
        LMLOG(LDBG_2, "get_network_address: Afi not supported (%d). It should never "
                "reach this point", lisp_addr_ip_afi(address));
        break;
    }

    return (network_address);
}

static inline lisp_addr_t *
pref_get_network_address_v4(lisp_addr_t *address)
{
    lisp_addr_t * network_address;
    int prefix_length;
    uint32_t mask = 0xFFFFFFFF;
    uint32_t addr;


    prefix_length = lisp_addr_get_plen(address);
    addr = ntohl(ip_addr_get_v4(lisp_addr_ip_get_addr(address))->s_addr);


    if (prefix_length != 0){
        mask = mask << (32 - prefix_length);
    }else{
        mask = 0;
    }
    addr = addr & mask;

    network_address = lisp_addr_new_lafi(LM_AFI_IP);

    addr = htonl(addr);
    ip_addr_set_v4(lisp_addr_ip_get_addr(network_address), &addr);

    return network_address;
}

static inline lisp_addr_t *
pref_get_network_address_v6(lisp_addr_t *address)
{
    lisp_addr_t * network_address;
    uint32_t addr32[4];
    struct in6_addr *addr;
    uint32_t mask[4] = {0,0,0,0};
    int prefix_length;
    int ctr = 0;
    int a,b;

    prefix_length = lisp_addr_get_plen(address);
    a = (prefix_length) / 32;
    b = (prefix_length) % 32;

    for (ctr = 0; ctr<a ; ctr++){
        mask[ctr] = 0xFFFFFFFF;
    }
    if (b != 0){
        mask[a] = 0xFFFFFFFF<<(32-b);
    }

    addr = ip_addr_get_v6(lisp_addr_ip_get_addr(address));
    for (ctr = 0 ; ctr < 4 ; ctr++){
        addr32[ctr] = htonl(ntohl(addr->s6_addr32[ctr]) & mask[ctr]);
    }
    network_address = lisp_addr_new_lafi(LM_AFI_IP);
    ip_addr_set_v6(lisp_addr_ip_get_addr(network_address), &addr32);
    return network_address;
}

/*
 * pref_get_network_prefix returns a prefix address from an IP prefix.
 * For instance:  10.0.1.1/8 -> 10.0.0.0/8
 */
lisp_addr_t *
pref_get_network_prefix(lisp_addr_t *address)
{
    lisp_addr_t * prefix_addr;
    prefix_addr = pref_get_network_address(address);
    if (!prefix_addr){
        return (NULL);
    }
    lisp_addr_set_plen(prefix_addr, lisp_addr_get_plen(address));

    return (prefix_addr);
}
