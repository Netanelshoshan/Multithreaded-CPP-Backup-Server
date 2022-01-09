#ifndef CONSTANTS_H
#define CONSTANTS_H

#define SERVER_VERSION 1
#define CLIENT_VERSION 1
#define MAX_LENGTH 1024

// OpCodes
#define BACKUP_FILE 100
#define GET_FILE 200
#define ERASE_FILE 201
#define GET_BACKUP_LIST 202

// Response codes
#define GET_FILE_SUC 210
#define GET_LIST_SUC 211
#define BACKUP_FILE_SUC 212
#define ERASE_FILE_SUC 212
#define FILE_NOT_FOUND 1001
#define NO_CONTENT 1002
#define INTERNAL_ERROR 1003

#endif //CONSTANTS_H

