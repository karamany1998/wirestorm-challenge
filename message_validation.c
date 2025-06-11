
#include "message_validation.h"




/* This function will compute the checksum of the message received.
 * This occurs when the sensitive bit in the options fields is set.
 */
uint16_t compute_checksum(char* input , uint32_t len)
{
    printf("---------------------------------------\n");
    uint32_t idx = 0; 
    printf("computing checksum.....\n");

    uint32_t checksum = 0 ; 
    uint32_t checksum_host = 0 ; 
    while(idx < len)
    {
        //the problem states that we should fill in checksum field with 0xcc 0xcc when computing it
        //just add it directly cuz I don't want to modify the string here then change it back afterwards
        if(idx == 4)
        {
            checksum += (uint16_t)0xcccc;
            idx+=2;
            continue;
        }
        //handle edge case(one byte left at the end)
        if(idx+1>=len)
        {
            checksum+= (unsigned char)input[idx];
            printf("odd-length string \n");
            break;
        }
        //copy over two chars(16-bit word) starting at index idx and add them to checksum
        //need to consider network-order to host-order so use ntohs
        uint16_t currVal = 0;
        memcpy(&currVal, &input[idx], 2);
        checksum += (currVal);
        idx += 2;
    }
    /*
     * We also need to consider any overflow because we are doing a 
     * one's compelemt sum. 
     * An easy way to do this is to add top-16 bits to 
     * lower 16-bits and keep looping until the top 16 bits are zero
     */

    uint32_t overflow = checksum>>16;
    printf("overflow is %u \n" , overflow);
    printf("lower 16-bits in checksum is %u \n", (uint16_t)(checksum&0xFFFF));
    while(overflow)
    {   
        
        checksum = overflow + (checksum & 0xFFFFU);
        overflow = checksum>>16;
        printf("overflow is %u \n" , overflow);
        printf("lower 16-bits in checksum is %u \n", (uint16_t)(checksum&0xFFFF));
        printf("-----------------------------------\n");
    }
    return ~((uint16_t)checksum);
}


/*
* Input: msg received and length of message
* Return: 1 if length correct according to header, 0 otherwise
*/
uint16_t validate_length(char* msg , uint16_t msg_len)
{

    if(msg_len < HEADER_LEN)
    {
        printf("message is less than 8 bytes(must be atleast >= header size)...Not Sending \n");
        return 0;
    }
    uint16_t length_network_order = 0;
    memcpy(&length_network_order , &msg[2] , 2);
    uint16_t lengthHeader = ntohs( length_network_order);

    printf("total length is %i \n " , lengthHeader);
    if(msg_len - HEADER_LEN != lengthHeader)
    {
        printf("length of data is incorrect..\n");
        printf("not sending...\n");
        return 0;
    }
    else
    {
        printf("message length is correct..\n");
        return 1;
    }
}

/*
* validates of the magic byte is correct in the header
*/
uint16_t validate_magic(char* msg)
{
    //validate magic byte
    if((unsigned char) msg[0] == 0xcc)
    {
        printf("received correct magic byte \n");
        return 1;
    }
    else 
    {
        printf("incorrect magic byte..Not sending message \n");
        return 0;
    }
}

uint16_t validate_checksum(char* msg , uint16_t msg_len)
{
    //check option field for the sensitive bit
    unsigned char opts = msg[1];
    //if 2nd bit is 1, then compute the checksum
    uint16_t check_checksum = (opts & 0b01000000);
    uint16_t ans = 1;
    if(check_checksum)
    {
        uint16_t checksum = compute_checksum(msg, msg_len);
        uint16_t real_checksum = 0;
        memcpy(&real_checksum , &msg[4] , 2);
        printf("need to calculate checksum....\n");
        printf("checksum is %u \n" , checksum);
        if(checksum != real_checksum)
        {
            printf("An error occurred in the message \n");
            printf("dropping..not sending due to checksum error \n");
            ans = 0;
        }
        else
        {
            printf("checksum is correct....\n");
        }
    }

    return ans;
}

