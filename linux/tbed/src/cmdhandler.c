#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../include/macros.h"
#include "../include/control-protocol.h"
#include "../include/prototypes.h"
#include "../include/console_connect.h"
#include "../include/global.h"

const char *SRV_IP;
char map_serv[15];
char ip_serv[15];
char map_brouter[15];
/*----------------------------------------------------------
 This function is used to handle operator commands.
*------------------------------------------------------------*/
struct c_msg  command_handler (char command[BUFLEN])
{
    struct c_msg message;
    memset (&message, 0, sizeof (struct c_msg));
    char array_token[2][20];
    char seps[]   = " ";
    char *token;
    char beagle[10];
    int num;
    int i;
    int flag = 0;
    i = 0;
    token = strtok (command, seps );

    /* While there are tokens in "command" */
    while ( token != NULL ) {
        strcpy (array_token[i], token);
        token = strtok ( NULL, seps); /* Get next token: */
        i++;
    }

    if (!strcmp (array_token[0], "restart") && !strcmp (array_token[1], "t")) {
        message.cmsgtyp = htonl (C_RESTART);

    } else if (!strcmp (array_token[0], "xon")) {
        if (!strcmp (array_token[1], "-all")) {
            message.cmsgtyp = htonl (C_XON);
            message.xonoffid = htonl (ALL);

        } else {
            message.cmsgtyp = htonl (C_XON);

            if (isnumeric (array_token[1]) == 1) {
                num = atoi (array_token[1]);

                if ((num >= 0) && (num <= 45)) {
                    message.xonoffid = htonl (num);

                } else {
                    fprintf (fp, "Incorrect ID\n");
                    message.cmsgtyp = htonl (C_ERR);
                }

            } else {
                for (i = 0; i < 46; i++) {
                    /* DEBUG *///printf("name:input %s:%s \n",map_list[i] , array_token[1]);
                    if (! (strcmp (map_list[i], array_token[1]))) {
                        message.xonoffid = htonl (i);
                        flag = 1;
                    }
                }

                if (flag == 0) {
                    fprintf (fp, "Incorrect Node Name\n");
                    message.cmsgtyp = htonl (C_ERR);
                }
            }
        }

    } else if (!strcmp (array_token[0], "xoff")) {
        if (!strcmp (array_token[1], "-all")) {
            message.cmsgtyp = htonl (C_XOFF);
            message.xonoffid = htonl (ALL);

        } else {
            message.cmsgtyp = htonl (C_XOFF);

            if (isnumeric (array_token[1]) == 1) {
                num = atoi (array_token[1]);

                if ((num >= 0) && (num <= 45)) {
                    message.xonoffid = htonl (num);

                } else {
                    fprintf (fp, "Incorrect ID\n");
                    message.cmsgtyp = htonl (C_ERR);
                }

            } else {
                for (i = 0; i < 46; i++) {
                    /*DEBUG *///printf("name:input %s:%s \n",map_list[i] , array_token[1]);
                    if (! (strcmp (map_list[i], array_token[1]))) {
                        message.xonoffid = htonl (i);
                        flag = 1;
                    }
                }

                if (flag == 0) {
                    fprintf (fp, "Incorrect Node Name\n");
                    message.cmsgtyp = htonl (C_ERR);
                }
            }
        }

    } else if ((!strcmp (array_token[0], "nping")) && strcmp (array_token[1], " ") && strcmp (array_token[1], "-all")) {
        message.cmsgtyp = htonl (C_PING_REQ);
        message.clength = htonl (sizeof (message));

        if (isnumeric (array_token[1]) == 1) {
            num = atoi (array_token[1]);

            if ((num >= 0) && (num <= 45)) {
                message.pingnodeid = htonl (num);

            } else {
                fprintf (fp, "Incorrect ID\n");
                message.cmsgtyp = htonl (C_ERR);
            }

        } else {
            for (i = 0; i < 46; i++) {
                /*DEBUG *///printf("name:input %s:%s \n",map_list[i] , array_token[1]);
                if (! (strcmp (map_list[i], array_token[1]))) {
                    message.pingnodeid = htonl (i);
                    flag = 1;
                }
            }

            if (flag == 0) {
                fprintf (fp, "Incorrect Node Name\n");
                message.cmsgtyp = htonl (C_ERR);
            }
        }

    } else if ((!strcmp (array_token[0], "nping")) && strcmp (array_token[1], " ") && (!strcmp (array_token[1], "-all"))) {
        message.cmsgtyp = htonl (C_PING_ALL);
        message.pingnodeid = htonl (ALL);

    } else if (!strcmp (array_token[0], "topdump")) {
        message.cmsgtyp = htonl (C_TOP_REQ);

    } else if (!strcmp (array_token[0], "newtop") && (strcmp (array_token[1], " "))) {
        message.cmsgtyp = htonl (C_NEW_TOP);
        message.flen = htonl (strlen (array_token[1]));
        strcpy ((char *)message.fname , array_token[1]);
        mapping_list ((char *)message.fname);

    } else if (!strcmp (array_token[0], "online")) {
        message.cmsgtyp = htonl (C_ONLINE);

    } else if (!strcmp (array_token[0], "offline")) {
        message.cmsgtyp = htonl (C_OFFLINE);

    } else if (!strcmp (array_token[0], "delay")) {
        int delay = atoi (array_token[1]);
        usleep (delay);
        message.cmsgtyp = htonl (C_ERR);

    } else if (!strcmp (array_token[0], "exit")) {
        fprintf (fp, "====Management app is closed=====\n");
        exit (1);

    } else if (!strcmp (array_token[0], "help")) {
        help();

    } else if (!strcmp (array_token[0], "ts_find")) {
        message.cmsgtyp = htonl (C_TS_REQ);

    } else if (!strcmp (array_token[0], "ts_check")) {
        message.cmsgtyp = htonl (C_TS_REQ);

    } else if (!strcmp (array_token[0], "download") && array_token[1] != NULL && array_token[2] != NULL) {
        if (!strcmp (array_token[1], "t")) {
            memset (beagle, 0, sizeof (beagle));
            strcpy (beagle, "beagle");
            strcat (beagle, array_token[2]);
            strcpy (map_serv, beagle);
            strcat (ip_serv, NETIP);
            strcat (ip_serv, array_token[2]);
            /*DEBUG */ //printf("ip: %s", ip_serv);
            download_img (array_token[3], "cortex", beagle, XINUSERVER);
            message.cmsgtyp = htonl (C_ERR);

        } else if (!strcmp (array_token[1], "b")) {
            memset (beagle, 0, sizeof (beagle));
            strcpy (beagle, "beagle");
            strcat (beagle, array_token[2]);
            strcpy (map_brouter, beagle);
            download_img (array_token[3], "cortex", beagle, XINUSERVER);
            message.cmsgtyp = htonl (C_ERR);

        } else if (!strcmp (array_token[1], "n")) {
            memset (beagle, 0, sizeof (beagle));
            strcpy (beagle, "beagle");
            strcat (beagle, array_token[2]);
            download_img (array_token[3], "cortex", beagle, XINUSERVER);
            message.cmsgtyp = htonl (C_ERR);
        }

    } else if (!strcmp (array_token[0], "pcycle")) {
        if (!strcmp (array_token[1], "t")) {
            powercycle_bgnd ("cortex", map_serv, XINUSERVER);
            SRV_IP = ip_serv;

            if (inet_aton (SRV_IP, &si_other.sin_addr) == 0) {
                fprintf (stderr, "inet_aton() failed\n");
                exit (1);
            }

            message.cmsgtyp = htonl (C_ERR);

        } else if (atoi (array_token[1]) > 101 && atoi (array_token[1]) < 184) {
            memset (beagle, 0, sizeof (beagle));
            strcpy (beagle, "beagle");
            strcat (beagle, array_token[1]);
            powercycle_bgnd ("cortex", beagle, XINUSERVER);
            message.cmsgtyp = htonl (C_ERR);
        }

    } else {
        fprintf (fp, "%s is not defined\n", array_token[0]);
        message.cmsgtyp = htonl (C_ERR);
    }

    return message;
}




