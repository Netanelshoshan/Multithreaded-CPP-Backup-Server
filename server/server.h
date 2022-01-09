#ifndef SERVER_H
#define SERVER_H

#include <cstdint>
#include <string>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

struct Response {
    // Header
    uint8_t version = 0;
    uint16_t status = 0;
    uint16_t nameLen = 0;
    std::string filename;

    // Payload
    uint32_t size = 0;
    boost::filesystem::ifstream payload;
};


struct Request {
    // Header
    uint32_t uid = 0;
    uint8_t version = 0;
    uint8_t op = 0;
    uint16_t name_len = 0;
    std::string filename;

    // Payload
    uint32_t size = 0;
    boost::filesystem::ofstream payload;
};


//server functions
void server(boost::asio::io_context &io_context, unsigned short port);
void session(tcp::socket sock);
bool isClient(std::list<uint32_t> list, uint32_t id);
bool isValid(Request *request);
void clear_buffer(uint8_t *buf, uint32_t length);
std::vector<uint8_t>
reply(Response *response, uint16_t status = 0, uint16_t nameLen = 0, std::string filename = "",
      uint32_t size = 0);
uint16_t parseRequest(tcp::socket &sock, Request *request, Response *response);
bool mkdir(uint32_t uid);
uint16_t backupFile(tcp::socket &sock, Request *request,Response *response);

uint16_t retrieveFileFromBackup(tcp::socket &sock, Request *request, Response *response);
uint16_t eraseFile(boost::filesystem::path path);
uint16_t sendBackupList(tcp::socket &sock,Request *request, Response *response, std::string fileName,
                        std::vector<std::string> fileList);
std::vector<std::string> getBackupList(boost::filesystem::path path);
std::string generateRandomAlphaNum(const int len);


#endif //SERVER_H
