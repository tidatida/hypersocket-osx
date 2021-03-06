//
//  main.c
//  RedirectCMD
//
//  Created by Lee Painter on 26/11/2013.
//  Copyright (c) 2013 Hypersocket Limited. All rights reserved.
//

#include <stdio.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/kern_control.h>
#include <sys/sys_domain.h>
#include <arpa/inet.h>

#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

#include "RedirectNKE.h"

static struct ctl_info ctl_info;
static struct sockaddr_ctl sc;
static int g_sock = -1;

static int connect_control_socket()
{
    g_sock = socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL);
	if (g_sock < 0) 
		return -1;
	
    bzero(&ctl_info, sizeof(struct ctl_info));
    strcpy(ctl_info.ctl_name, KERNCTLID);
    
    if (ioctl(g_sock, CTLIOCGINFO, &ctl_info) == -1) 
		return -1;
    
	bzero(&sc, sizeof(struct sockaddr_ctl));
	sc.sc_len = sizeof(struct sockaddr_ctl);
	sc.sc_family = AF_SYSTEM;
	sc.ss_sysaddr = SYSPROTO_CONTROL;
	sc.sc_id = ctl_info.ctl_id;
	sc.sc_unit = 0;
    
	if (connect(g_sock, (struct sockaddr *)&sc, sizeof(struct sockaddr_ctl))) 
        return -1;
    
    return 0;
    
}

static void close_control_socket()
{
    if(g_sock >= 0)
        close(g_sock);
    g_sock = -1;
}

static bool ensure_control_socket_is_open()
{
    if(g_sock < 0)
        connect_control_socket();
    
    return g_sock >= 0;
    
}

static int add_rule(const char* ip, unsigned short port, const char* forward_to, unsigned short forward_port)
{
    if(!ensure_control_socket_is_open())
        return -1;
    
    struct in_addr network;
    struct in_addr forward;
    
    inet_aton(forward_to, &forward);
    inet_aton(ip, &network);
    
    socklen_t size = (sizeof(in_addr_t)*2) + (sizeof(unsigned short)*2);
    void* tmp = malloc(size);
    void* tmp2 = tmp;
    memcpy(tmp2, &network.s_addr, sizeof(in_addr_t));
    tmp2+=sizeof(in_addr_t);
    memcpy(tmp2, &port, sizeof(unsigned short));
    tmp2+=sizeof(unsigned short);
    memcpy(tmp2, &forward.s_addr, sizeof(in_addr_t));
    tmp2+=sizeof(in_addr_t);
    memcpy(tmp2, &forward_port, sizeof(unsigned short));
    
    int retval = 1;
    
    if(setsockopt(g_sock, SYSPROTO_CONTROL, REDIRECTNKE_ADD_REDIRECTION, tmp, size) != 0)
        goto remove_exit;
    
    retval = 0;
    
remove_exit:
    
    free(tmp);
    return retval;
    
}

static int remove_rule(const char* ip, unsigned short port, const char* forward_to, unsigned short forward_port)
{
    if(!ensure_control_socket_is_open())
        return -1;
    
    struct in_addr network;
    struct in_addr forward;
    
    inet_aton(forward_to, &forward);
    inet_aton(ip, &network);
    
    socklen_t size = (sizeof(in_addr_t)*2) + (sizeof(unsigned short)*2);
    void* tmp = malloc(size);
    void* tmp2 = tmp;
    memcpy(tmp2, &network.s_addr, sizeof(in_addr_t));
    tmp2+=sizeof(in_addr_t);
    memcpy(tmp2, &port, sizeof(unsigned short));
    tmp2+=sizeof(unsigned short);
    memcpy(tmp2, &forward.s_addr, sizeof(in_addr_t));
    tmp2+=sizeof(in_addr_t);
    memcpy(tmp2, &forward_port, sizeof(unsigned short));
    
    int retval = 1;
    
    if(setsockopt(g_sock, SYSPROTO_CONTROL, REDIRECTNKE_REMOVE_REDIRECTION, tmp, size) != 0)
        goto remove_exit;
    
    retval = 0;
    
remove_exit:
    
    free(tmp);
    return retval;
}

/*static int lookup_original_destination(const char* source_addr, unsigned short source_port, 
                                       const char* forward_to, unsigned short forward_port,
                                       struct in_addr* dest_ip, unsigned short* dest_port) {
    if(!ensure_control_socket_is_open())
        return -1;
    
    struct in_addr source_ip;
    struct in_addr forward_ip;
    
    inet_aton(source_addr, &source_ip);
    inet_aton(forward_to, &forward_ip);
    
    void* tmp = malloc((sizeof(in_addr_t)*2) + (sizeof(unsigned short) * 2));
    void* tmp2 = tmp;
    memcpy(tmp2, &source_ip.s_addr, sizeof(in_addr_t));
    tmp2+=sizeof(in_addr_t);
    memcpy(tmp2, &source_port, sizeof(unsigned short));
    tmp2+=sizeof(unsigned short);
    memcpy(tmp2, &forward_ip.s_addr, sizeof(in_addr_t));
    tmp2+=sizeof(in_addr_t);
    memcpy(tmp2, &forward_port, sizeof(unsigned short));
    
    int retval = 1;
    
    socklen_t size = (sizeof(in_addr_t)*2) + (sizeof(unsigned short) * 2);
    if (getsockopt(g_sock, SYSPROTO_CONTROL, REDIRECTNKE_GET_ORIGINAL_DESTINATION, tmp, &size) != 0) {
        goto lookup_exit;
    }
    
    retval = 0;
    
    *dest_ip = *(struct in_addr*)tmp;
    *dest_port = ntohs(*(unsigned short *)(tmp+sizeof(in_addr_t))); 
    
    printf("Redirecting %s:%d\n", inet_ntoa(*(struct in_addr*)dest_ip), (int)dest_port);
    
lookup_exit:
    free(tmp);
    return retval;
}*/

static int update_settings(bool log_redirected_packets, bool log_other_packets)
{
    if(!ensure_control_socket_is_open())
        return -1;
    
    socklen_t size = (sizeof(int)*2);
    
    int t1 = log_redirected_packets ? 1 : 0;
    int t2 = log_other_packets ? 1 : 0;
    
    void* tmp = malloc(size);
    void* tmp2 = tmp;
    memcpy(tmp2, &t1, sizeof(int));
    tmp2+=sizeof(int);
    memcpy(tmp2, &t2, sizeof(int));
    
    int retval = 1;
    if (setsockopt(g_sock, SYSPROTO_CONTROL, REDIRECTNKE_UPDATE_SETTINGS, tmp, size) != 0) {
        goto update_settings_exit;
    }
    
    retval = 0;
    
update_settings_exit:
    
    free(tmp);
    return retval;
    
}

void usage()
{
    printf("usage: redirect add|remove <ip> <port> <forward_to_ip> <forward_to_port>\n");
    printf("       redirect log on|off\n");
}

int main(int argc, const char * argv[])
{
    int ret = 1;

    for(int i=0;i<argc;i++) {
        printf("%s\n", argv[i]);
    }
    ret = connect_control_socket();
    
    if(ret==0)
    {
        if(argc==3 || argc ==6)
        {
            if(argc==3)
            {
                if(strcmp("log", argv[1])==0)
                {
                    bool on = strcmp("on", argv[2])==0;
                    ret = update_settings(on, on);
                }
                else {
                    usage();
                }
                
            } else {
                if(strcmp("add", argv[1])==0) 
                {
                    ret = add_rule(argv[2], strtoul(argv[3],NULL,0), argv[4], strtoul(argv[5],NULL,0));
                } 
                else if(strcmp("remove", argv[1])==0)
                {
                    ret = remove_rule(argv[2], strtoul(argv[3],NULL,0), argv[4], strtoul(argv[5],NULL,0));
                } 
                else {
                    usage();
                }

            }

            if(ret!=0)
            {
                printf("Failed to %s rule\n", argv[1]);
                
            }
            
            close_control_socket();
        }
        else {
            usage();
        }
    }
    else {
       printf("Failed to connect to kernel extension. Are you running with privileges?\n");
    }
        
    if(ret==0)
    {
        
    }
    else {
        
    }
    
    return ret;
}

