#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
/*
56 69 56 4F 74 65 63 68 32 00 64 2B 00 1B 68 65 6C 6C 6F 77 6F 72 6C 64 00 31 30 30 30 00 30 00 30 31 32 34 35 36 37 38 39 00 00

    IDG V2 Command Frame Format
    Byte0-9  | Byte 10 | Byte 11 | Byte 12 | Byte 13 | Byte 14-(14+n-1)|
    Byte14+n | Byte 15+n
    Header   | Command | Subcmd  | Len MSB | Len LSB | Data | CRC LSB |
    CRC MSB

*/

#define FILE_TYPE ".zip"
#define IDG_HEADER_V2 "ViVOtech2\0"
#define IDG_HEADER_LEN 10
#define V2_CMD_IDX 10
#define V2_SUBCMD_IDX 11
#define v2_DL_MSB_OFFSET 12
#define v2_DL_LSB_OFFSET 13
#define V2_APK_NAME_IDX 14
#define NUM_OF_FLAG_BYTES 2
#define CMD 0X64
#define SUBCMD 0X2B
#define MAX_DATA_LEN 1000

/********************************************************************************

Flag to indicate the endpoints of file transmission, zero-terminated ASCII
0 = Neither first or last packet
1 = Last packet
2 = First packet
3 = First and last packet: set for one packet <= 1000 bytes

***********************************************************************************/

enum file_transfer_flag_e
{
    FILE_TRANSFER_FLAG_NOT_FIRST_NOR_LAST_PACKET = 0x00,
    FILE_TRANSFER_FLAG_LAST_PACKET,
    FILE_TRANSFER_FLAG_FIRST_PACKET,
    FILE_TRANSFER_FLAG_FIRST_AND_LAST_PACKET
};

/****************************************************************************
 * Private Types
 ****************************************************************************/
/****************************************************************************
 * Private Data
 ***************************************************************************/

size_t result;
char *buffer = NULL;

/***************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

long get_file_size(char *filename)
{
    long file_size;
    FILE *file = fopen(filename, "rb");
    if (file == NULL)
    {
        printf("Error opening file : %s\n", strerror(errno));
        return -1;
    }
    // Get the file size
    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    rewind(file);
    fclose(file);
    return file_size;
}

int main(int argc, char *argv[])

{
    if(argc != 2)
    {
        printf("Invalid arguments");
        return 1;
    }

    char *filename = argv[1];      //file name cannot be longer tha 128 bytes as per IDG protocol
    char *ext = strrchr(filename, '.');
    if (ext != NULL)
    {
        if (strcmp(ext, FILE_TYPE) != 0)
        {
            printf("improper file type\n");
            return 1;
        }
    }
    else
    {
        printf("No file extension found\n");
        return 1;
    }
    int chunk_data_len;
    long application_size = 0;
    int num_of_packets;
    int buffer_len = 0;
    long file_size;
    char application_name[128]; // Application_Name name cannot be longer tha 128 bytes as per IDG protocol
    char application_file_size[9]; // Application_File_Size name cannot be longer tha 9 bytes as per IDG protocol
    char data_packet_size[4]; // Application_File_Size name cannot be longer tha 9 bytes as per IDG protocol
    char flag[2]; // Flag to indicate the endpoints of file transmission, zero-terminated ASCII
    char rrc_download_application_idg_command[1024]; // Max data to transfer using docklight is 1024 bytes
    int V2_APK_SIZE_IDX;
    int V2_APK_DATA_IDX;
    uint16_t data_len = 0;

    FILE *in;           //to read
    FILE *out;

    sscanf(filename, "%[^.]", application_name);
    memcpy(rrc_download_application_idg_command, IDG_HEADER_V2, IDG_HEADER_LEN); // write ViVOtech2\0 in buffer
    rrc_download_application_idg_command[V2_CMD_IDX] = CMD; // write CMD in buffer
    rrc_download_application_idg_command[V2_SUBCMD_IDX] = SUBCMD; // write SUBCMD in buffer
    rrc_download_application_idg_command[v2_DL_MSB_OFFSET] = 0; // write v2_DL_MSB_OFFSET as 0 in buffer
    rrc_download_application_idg_command[v2_DL_LSB_OFFSET] = 0; // write v2_DL_LSB_OFFSET as 0 in buffer
    memcpy(&rrc_download_application_idg_command[V2_APK_NAME_IDX], application_name, strlen(application_name)); // write application name in buffer
    rrc_download_application_idg_command[V2_APK_NAME_IDX + strlen(application_name)] = '\0'; // Add null after application name in buffer

    V2_APK_SIZE_IDX = V2_APK_NAME_IDX + strlen(application_name) + 1;
    buffer_len = V2_APK_SIZE_IDX;
    application_size = get_file_size(filename);
    if (application_size == -1)
    {
        printf("Invalid file");
        return 1;
    }

    sprintf(application_file_size, "%d", application_size);
    memcpy(&rrc_download_application_idg_command[V2_APK_SIZE_IDX], application_file_size, strlen(application_file_size));
    rrc_download_application_idg_command[V2_APK_SIZE_IDX + strlen(application_file_size)] = '\0'; // Add null after application name in buffer
    V2_APK_DATA_IDX = V2_APK_SIZE_IDX + strlen(application_file_size) + 1;
    buffer_len = V2_APK_DATA_IDX;

    printf("apk file size if %s", application_file_size);
    chunk_data_len = MAX_DATA_LEN - (strlen(application_name) + strlen(application_file_size) + NUM_OF_FLAG_BYTES + 2);
    printf("\n chunk_data_len %d", chunk_data_len);
    num_of_packets = application_size / chunk_data_len;
    if (application_size % chunk_data_len != 0)
    {
        num_of_packets += 1;
    }
    printf("\n num_of_packets %d", num_of_packets);

    int version_index = strlen(application_name);

    printf("\n version_index %d", version_index);
    for (int i = 0; i < num_of_packets; i++)
    {
        char temp[2];
        sprintf(temp,"%d",i);
        strcat(application_name, temp);        
        strcat(application_name, ".bin");
        printf("\n %s", application_name);

        out = fopen(application_name, "wb");
        if (out == NULL)
        {
            printf("Error creating the file.\n");
            return 1;
        }

        result = fwrite(rrc_download_application_idg_command, 1, buffer_len, out);
        if (result != buffer_len)
        {
            printf("Error writing file.\n");
            return 1;
        }

        // Close the file
        fclose(out);

        application_name[version_index] = 0;       // clear file name
    }

#if 0

    // Open the split file

    FILE* file = fopen(filename, "rb");

    if (file == NULL) {

        printf("Error opening file : %s\n", strerror(errno));

        return 1;

    }

 

    // Get the file size

    fseek(file, 0, SEEK_END);

    file_size = ftell(file);

    rewind(file);

 

    // Allocate memory for the buffer

    buffer = (char*)malloc(file_size);

    if (buffer == NULL) {

        printf("Memory error.\n");

        return 1;

    }

 

    result = fread(buffer, 1, file_size, file);

    if (result != file_size) {

        printf("Error reading file.\n");

        return 1;

    }

 

    fclose(file);

 

    file = fopen("hellobin.bin", "wb");

 

    if (file == NULL) {

        printf("Error creating the file.\n");

        return 1;

    }

 

    result = fwrite(buffer, 1, file_size  , file);

    if (result != file_size) {

        printf("Error writing file.\n");

        return 1;

    }

 

    // Close the file

    fclose(file);

 

    // Do something with the buffer here

 

    // Free the buffer memory

    free(buffer);

#endif

    return 0;
}
