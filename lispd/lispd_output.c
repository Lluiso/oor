/*
 * lispd_output.c
 *
 * This file is part of LISP Mobile Node Implementation.
 *
 * Copyright (C) 2012 Cisco Systems, Inc, 2012. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * Please send any bug reports or fixes you make to the email address(es):
 *    LISP-MN developers <devel@lispmob.org>
 *
 * Written or modified by:
 *    Alberto Rodriguez Natal <arnatal@ac.upc.edu>
 */




#include "lispd_map_request.h"
#include "lispd_output.h"





void add_ipv4_header ( char *position,
                 char *original_packet_position,
                 lisp_addr_t *src_addr,
                 lisp_addr_t *dst_addr) {


    struct iphdr *iph;
    struct iphdr *inner_iph;


    /*
     * Construct and add the outer ip header
     */

    iph = ( struct iphdr * ) position;
    inner_iph = ( struct iphdr * ) original_packet_position;


    //arnatal TODO: check inner ip header version to proper copy tos and ttl fields

    iph->version  = 4;
    iph->ihl      = sizeof ( struct iphdr ) >>2;
    iph->frag_off = 0;   // XXX recompute above, use method in 5.4.1 of draft
    iph->protocol = IPPROTO_UDP;

    iph->tos      = inner_iph->tos;

    iph->daddr    = dst_addr->address.ip.s_addr;
    iph->saddr    = src_addr->address.ip.s_addr;

    iph->ttl      = inner_iph->ttl;


    //arnatal XXX: Checksum?

    //iph->check = ip_checksum((uint16_t*) iph, sizeof(struct iphdr));

    //if ((udpsum = udp_checksum(udh, pckt_length - iph_len, packet_buf, AF_INET)) == -1) {
    //    return (0);
    //}
    //udh->check = udpsum;

}

void add_udp_header(char *position,
              int length,
              int src_port,
              int dst_port){

    struct udphdr *udh;


    /*
     * Construct and add the udp header
     */
    udh = ( struct udphdr * ) position;

    /*
     * Hash of inner header source/dest addr. This needs thought.
     */
    udh->source = htons ( src_port ); //arnatal TODO: Selec source port based on tuple?
    udh->dest =  htons ( dst_port );
    udh->len = htons ( sizeof ( struct udphdr ) + length );
    //udh->len = htons(sizeof(struct udphdr)); /* Wireshark detects this as error*/
    udh->check = 0; // SHOULD be 0 as in LISP ID


}

void add_lisp_header(char *position,
                     int iid){

    struct lisphdr *lisphdr;

    lisphdr = (struct lisphdr *) position;

    lisphdr->instance_id = iid;

    /* arnatal TODO: support for the rest of values*/
    lisphdr->echo_nonce = 0;
    lisphdr->lsb = 0;
    lisphdr->lsb_bits = 0;
    lisphdr->map_version = 0;
    lisphdr->nonce[0] = 0;
    lisphdr->nonce[1] = 0;
    lisphdr->nonce[2] = 0;
    lisphdr->nonce_present = 0;
    lisphdr->rflags = 0;

}

int encapsulate_packet(char *original_packet,
                    int original_packet_length,
                    int encap_afi,
                    lisp_addr_t *src_addr,
                    lisp_addr_t *dst_addr,
                    int src_port,
                    int dst_port,
                    int iid,
                    char **encap_packet,
                    int  *encap_packet_size){


    int extra_headers_size = 0;
    char *new_packet = NULL;

    int iphdr_len = 0;
    int udphdr_len = 0;
    int lisphdr_len = 0;

    switch (encap_afi){
        case AF_INET:
            iphdr_len = sizeof(struct iphdr);
            break;
        case AF_INET6:
            //arnatal TODO: write IPv6 support
            break;
    }
    
    
    udphdr_len = sizeof(struct udphdr);
    lisphdr_len = sizeof(struct lisphdr);

    extra_headers_size = iphdr_len + udphdr_len + lisphdr_len;

    new_packet = (char *) malloc (original_packet_length + extra_headers_size);

    if (new_packet == NULL){
        lispd_log_msg(LOG_ERR, "Can not IPv4 encap packet ");
        return BAD;
    }

    memset(new_packet,0,original_packet_length+extra_headers_size);

    memcpy (new_packet + extra_headers_size, original_packet, original_packet_length);



    add_lisp_header((char *)(new_packet + iphdr_len + udphdr_len), iid);

    add_udp_header((char *)(new_packet + iphdr_len),original_packet_length+lisphdr_len,src_port,dst_port);

    
    switch (encap_afi){
        case AF_INET:
            add_ipv4_header((char *)(new_packet),original_packet,src_addr,dst_addr);
            break;
        case AF_INET6:
            //arnatal TODO: write IPv6 support
            break;
    }

    *encap_packet = new_packet;
    *encap_packet_size = extra_headers_size + original_packet_length;

    return GOOD;
}



int send_by_raw_socket ( lispd_iface_elt *iface, char *packet_buf, int pckt_length ) {

    int socket;

    struct sockaddr *dst_addr;
    int dst_addr_len;
    struct sockaddr_in dst_addr4;
    //struct sockaddr_in6 dst_addr6;
    struct iphdr *iph;
    int nbytes;


    memset ( ( char * ) &dst_addr, 0, sizeof ( dst_addr ) );


    iph = ( struct iphdr * ) packet_buf;

    if ( iph->version == 4 ) {
        memset ( ( char * ) &dst_addr4, 0, sizeof ( dst_addr4 ) );
        dst_addr4.sin_family = AF_INET;
        dst_addr4.sin_port = htons ( LISP_DATA_PORT );
        dst_addr4.sin_addr.s_addr = iph->daddr;

        dst_addr = ( struct sockaddr * ) &dst_addr4;
        dst_addr_len = sizeof ( struct sockaddr_in );
        socket = iface->out_socket_v4;
    } else {
        return GOOD;
        //arnatal TODO: write IPv6 support
    }

    nbytes = sendto ( socket,
                      ( const void * ) packet_buf,
                      pckt_length,
                      0,
                      dst_addr,
                      dst_addr_len );

    if ( nbytes != pckt_length ) {
        lispd_log_msg( LOG_DAEMON, "send_by_raw_socket: send failed %s", strerror ( errno ) );
        return ( BAD );
    }

    return GOOD;

}

int fordward_native( lispd_iface_elt *iface, char *packet_buf, int pckt_length ){

    int ret;
    
    lispd_log_msg(LOG_INFO, "Fordwarding native for destination %s",
                        get_char_from_lisp_addr_t(extract_dst_addr_from_packet(packet_buf)));

    if(send_by_raw_socket(iface,packet_buf,pckt_length) != GOOD){
        ret = BAD;
    }else{
        ret = GOOD;
    }
    
    return ret;
    
}


int fordward_to_petr(lispd_iface_elt *iface, char *original_packet, int original_packet_length, int afi){

    lisp_addr_t *petr;
    lisp_addr_t *outer_src_addr;
    char *encap_packet;
    int  encap_packet_size;
    
    petr = get_proxy_etr(afi); 
    
    if (petr == NULL){
        lispd_log_msg(LOG_ERR, "Proxy-etr not found");
        return BAD;
    }

    lispd_log_msg(LOG_DEBUG, "Proxy-etr found: %s",get_char_from_lisp_addr_t(*petr));
    
    switch (afi){
        case AF_INET:
            outer_src_addr = iface->ipv4_address;
            break;
        case AF_INET6:
            //arnatal TODO: write IPv6 support
            break;
    }

    if (encapsulate_packet(original_packet,
                            original_packet_length,
                            afi,
                            outer_src_addr,
                            petr,
                            LISP_DATA_PORT,
                            LISP_DATA_PORT,
                            0,
                            &encap_packet,
                            &encap_packet_size) != GOOD){
        return BAD;
    }
    
    if (send_by_raw_socket (iface,encap_packet,encap_packet_size ) != GOOD){
        free (encap_packet );
        return BAD;
    }

    lispd_log_msg(LOG_INFO, "Fordwarded eid %s to petr",get_char_from_lisp_addr_t(extract_dst_addr_from_packet(original_packet)));
    free (encap_packet );
    
    return GOOD;
}

lisp_addr_t extract_dst_addr_from_packet ( char *packet ) {
    lisp_addr_t addr;
    struct iphdr *iph;
    struct ip6_hdr *ip6h;

    iph = (struct iphdr *) packet;

    if (iph->version == 4 ) {
        addr.afi = AF_INET;
        addr.address.ip.s_addr = iph->daddr;


    } else {
        ip6h = (struct ip6_hdr *) packet;
        addr.afi = AF_INET6;
        addr.address.ipv6 = ip6h->ip6_dst;
    }

    //arnatal TODO: check errors (afi unsupported)

    return addr;
}


lisp_addr_t extract_src_addr_from_packet ( char *packet ) {
    lisp_addr_t addr;
    struct iphdr *iph;
    struct ip6_hdr *ip6h;
    
    iph = (struct iphdr *) packet;
    
    if ( iph->version == 4 ) {
        addr.afi = AF_INET;
        addr.address.ip.s_addr = iph->saddr;
        
        
    } else {
        ip6h = (struct ip6_hdr *) packet;
        addr.afi = AF_INET6;
        addr.address.ipv6 = ip6h->ip6_src;
    }
    
    //arnatal TODO: check errors (afi unsupported)
    
    return addr;
}

int handle_map_cache_miss(lisp_addr_t *requested_eid, lisp_addr_t *src_eid){

    lispd_map_cache_entry *entry;
    timer_map_request_argument *arguments;

    if ((arguments = malloc(sizeof(timer_map_request_argument)))==NULL){
        lispd_log_msg(LOG_DEBUG,"handle_map_cache_miss: Can't allocate timer_map_request_argument -"
                "malloc: %s",strerror(errno));
        return (ERR_MALLOC);
    }


    //arnatal TODO: check if this works
    entry = new_map_cache_entry(
            *requested_eid,
            get_prefix_len(requested_eid->afi),
            DYNAMIC_MAP_CACHE_ENTRY,
            DEFAULT_DATA_CACHE_TTL);

    arguments->map_cache_entry = entry;
    arguments->src_eid = *src_eid;

    if ((err=send_map_request_miss(NULL, (void *)arguments))!=GOOD)
        return BAD;

    return GOOD;
}

lisp_addr_t *get_proxy_etr(int afi){
    lispd_weighted_addr_list_t *petr_list_elt;
    if(proxy_etrs!=NULL){
        petr_list_elt = proxy_etrs;
        while (proxy_etrs!=NULL){
            if (petr_list_elt->address->afi == afi)
                return petr_list_elt->address;
            petr_list_elt = petr_list_elt->next;
        }
    }
    return NULL;
}

lisp_addr_t *get_default_locator_addr(lispd_map_cache_entry *entry, int afi){

    lisp_addr_t *addr;
    
    switch(afi){ 
        case AF_INET:
            addr = entry->identifier->head_v4_locators_list->locator->locator_addr;
            break;
        case AF_INET6:
            addr = entry->identifier->head_v6_locators_list->locator->locator_addr;
            break;
    }

    return addr;
}


int is_lisp_packet(char *packet, int packet_length){

    struct iphdr *iph = NULL;
    struct ip6_hdr *ip6h = NULL;
    int ipXh_len = 0;
    int lvl4proto = 0;
    struct udphdr *udh = NULL;

    iph = (struct iphdr *) packet;

    if (iph->version == 4 ) {
        lvl4proto = iph->protocol;
        ipXh_len = sizeof(struct iphdr);

    } else {
        ip6h = (struct ip6_hdr *) packet;
        lvl4proto = ip6h->ip6_nxt; //arnatal XXX: Supposing no extra headers
        ipXh_len = sizeof(struct ip6_hdr);

    }
    /*
     * Don't encapsulate LISP messages
     */

    if (lvl4proto != IPPROTO_UDP) {
        return FALSE;
    }

    udh = (struct udphdr *)(packet + ipXh_len);

    /*
     * If either of the udp ports are the control port or data, allow
     * to go out natively. This is a quick way around the
     * route filter which rewrites the EID as the source address.
     */
    if ((ntohs(udh->dest) != LISP_CONTROL_PORT) &&
            (ntohs(udh->source) != LISP_CONTROL_PORT) &&
            (ntohs(udh->source) != LISP_DATA_PORT) &&
            (ntohs(udh->dest) != LISP_DATA_PORT) ) {

        return FALSE;
    }

    return TRUE;
}




int lisp_output ( char *original_packet, int original_packet_length ) {
    lispd_iface_elt *iface;
    
    char *encap_packet = NULL;
    int  encap_packet_size = 0;
    lisp_addr_t *outer_dst_addr = NULL;
    int map_cache_query_result = 0;
    lisp_addr_t *outer_src_addr = NULL;
    lisp_addr_t original_src_addr;
    lisp_addr_t original_dst_addr;
    lispd_map_cache_entry *entry = NULL;
    
    int default_encap_afi = 0;


    
    //arnatal TODO TODO: Check if local -> Do not encapsulate (can be solved with proper route configuration)
    //arnatal: Do not need to check here if route metrics setted correctly -> local more preferable than default (tun)
    
    
    original_src_addr = extract_src_addr_from_packet(original_packet);
    original_dst_addr = extract_dst_addr_from_packet(original_packet);

    lispd_log_msg(LOG_DEBUG,"Packet received dst. to: %s\n",get_char_from_lisp_addr_t(original_dst_addr));
    
    default_encap_afi = original_dst_addr.afi; //arnatal TODO: Choose proper encapsulation afi

    /* No complete IPv6 support yet */

    if (default_encap_afi == AF_INET6){
        return (fordward_native(get_default_output_iface(default_encap_afi),
                                original_packet,
                                original_packet_length));
    }

    /* If already LISP packet, do not encapsulate again */
    
    if (is_lisp_packet(original_packet,original_packet_length) == TRUE){
        return (fordward_native(get_default_output_iface(default_encap_afi),
                                original_packet,
                                original_packet_length));
    }

    /* If received packet doesn't have a source EID, forward it natively */
    if (lookup_eid_in_db (original_src_addr) == NULL){
        return (fordward_native(get_default_output_iface(default_encap_afi),
                                           original_packet,
                                           original_packet_length));
    }


    map_cache_query_result = lookup_eid_cache(original_dst_addr,&entry);
    
    //arnatal XXX: is this the correct error type?
    if (map_cache_query_result == ERR_DB){ /* There is no entry in the map cache */
        lispd_log_msg(LOG_INFO, "No map cache retrieved for eid %s",get_char_from_lisp_addr_t(original_dst_addr));
        handle_map_cache_miss(&original_dst_addr, &original_src_addr);
    }
    /* Packets with negative map cache entry, no active map cache entry or no map cache entry are forwarded to PETR */
    if ((map_cache_query_result != GOOD) || (entry->active == NO_ACTIVE) || (entry->identifier->locator_count == 0) ){ /* There is no entry or is not active*/
        
        /* Try to fordward to petr*/
        if (fordward_to_petr(get_default_output_iface(default_encap_afi), /* Use afi of original dst for encapsulation */
                             original_packet,
                             original_packet_length,
                             default_encap_afi) != GOOD){
            /* If error, fordward native*/
            return (fordward_native(get_default_output_iface(default_encap_afi),
                                    original_packet,
                                    original_packet_length));
            }
            return GOOD;
    }
    
    /* There is an entry in the map cache */
    
    iface = get_default_output_iface(default_encap_afi);
    

    outer_src_addr = get_iface_address(iface,default_encap_afi);
    outer_dst_addr = get_default_locator_addr(entry,default_encap_afi);
        
    
    encapsulate_packet(original_packet,
                       original_packet_length,
                       default_encap_afi,
                       outer_src_addr,
                       outer_dst_addr,
                       LISP_DATA_PORT,
                       LISP_DATA_PORT,
                       entry->identifier->iid,
                       &encap_packet,
                       &encap_packet_size);
    
    send_by_raw_socket (iface,encap_packet,encap_packet_size);
    
    free (encap_packet);
    
    return GOOD;
}

void process_output_packet ( int fd, char *tun_receive_buf, unsigned int tun_receive_size ) {
    int nread;
    
    nread = read ( fd, tun_receive_buf, tun_receive_size );
    
    lisp_output ( tun_receive_buf, nread );
}