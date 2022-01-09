#include <cstdlib>
#include <cmath>
#include <iostream>
#include <utility>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/random/random_device.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include "constants.h"
#include "server.h"

using boost::asio::ip::tcp;

std::list<uint32_t> clients; // will hold the ID's of all the clients that were connected


/* process client's session */
void session(tcp::socket sock) {

    boost::system::error_code error;
    uint8_t data[MAX_LENGTH] = {0};
    uint32_t offset = 0;
    //size_t length;

    auto *request = new Request;
    auto *response = new Response;

    try {
        // read some data (header)
        sock.read_some(boost::asio::buffer(data), error);

        /* insert data into header fields */
        request->uid = data[3];
        request->uid = (request->uid << 8) + data[2];
        request->uid = (request->uid << 8) + data[1];
        request->uid = (request->uid << 8) + data[0];

        request->version = data[4];

        request->op = data[5];

        request->name_len = data[7];
        request->name_len = (request->name_len << 8) + data[6];

        /* copy the filename. only copy name_len amount of bytes */
        for (offset = 8; offset < (8 + request->name_len); offset++)
            request->filename += data[offset];

        request->size = data[offset + 3];
        request->size = (request->size << 8) + data[offset + 2];
        request->size = (request->size << 8) + data[offset + 1];
        request->size = (request->size << 8) + data[offset + 0];

        /* if it's a new client */
        if (!isClient(clients, request->uid))
            clients.push_back(request->uid);

        /* if request isn't valid */
        if (!isValid(request)) {
            throw boost::system::system_error(error);
        }
        std::cout << "user id: " << std::to_string(request->uid) << std::endl;

        /* process the request  */
        response->status = parseRequest(sock, request, response);

        if (error)
            throw boost::system::system_error(error);
    }
    catch (std::exception &e) {
        std::cerr << "Exception in thread, session: " << e.what() << "\n";
    }
    std::cout << "Session ended with status: " << std::to_string(response->status) << std::endl;

    // making sure to delete the allocated objects for this session.
    delete (request);
    delete (response);
}

/* checks if we got valid request */
bool isValid(Request *request) {
    if (request->version != CLIENT_VERSION)
        return false;

    if (request->op != BACKUP_FILE &&
        request->op != GET_FILE &&
        request->op != ERASE_FILE &&
        request->op != GET_BACKUP_LIST)
        return false;

    if (request->name_len != request->filename.size())
        return false;

    return true;
}

/* parse the request */
uint16_t parseRequest(tcp::socket &sock, Request *request, Response *response) {
    uint16_t retCode = 0;
    response->status = 0;
    boost::filesystem::path path(boost::filesystem::current_path());
    std::string fileName = "";
    std::vector<uint8_t> header;
    std::vector<std::string> fileList;

    try {
        // different cases for different client operations
        switch (request->op) {

            case BACKUP_FILE:
                std::cout << "Backing up: " << request->filename << std::endl;

                // making sure file size doesn't exceed 2^32
                if (request->size > pow(2, 32)) {
                    std::cout << "File size to large (larger than 2^32 bytes)" << std::endl;
                    header = reply(response, INTERNAL_ERROR);
                    boost::asio::write(sock, boost::asio::buffer(header));
                    return INTERNAL_ERROR;
                }

                // check if there's backup directory for the client
                if (!mkdir(request->uid)) {
                    std::cout << "Error opening client's directory" << std::endl;
                    header = reply(response, INTERNAL_ERROR);
                    boost::asio::write(sock, boost::asio::buffer(header));
                    return INTERNAL_ERROR;
                }

                response->status = backupFile(sock, request, response);

                header = reply(response, response->status, request->name_len, response->filename);
                boost::asio::write(sock, boost::asio::buffer(header));
                break;

            case GET_FILE:
                std::cout << "Retrieving file from backup: " << request->filename << std::endl;

                path.append(std::to_string(request->uid));

                // check if client's directory actually exists
                if (!boost::filesystem::exists(path) && !boost::filesystem::is_directory(path)) {
                    std::cout << "There's no files on the server for uid: " << request->uid << std::endl;
                    header = reply(response, NO_CONTENT);
                    boost::asio::write(sock, boost::asio::buffer(header));
                    return NO_CONTENT;
                }

                path.append(request->filename);

                // check if the required file exists
                if (!boost::filesystem::exists(path)) {
                    std::cout << "Client's file does not exist" << std::endl;
                    header = reply(response, FILE_NOT_FOUND, request->name_len, response->filename);
                    boost::asio::write(sock, boost::asio::buffer(header));
                    return FILE_NOT_FOUND;
                }

                // check if the file is not empty
                if (boost::filesystem::file_size(path) == 0) {
                    std::cout << "Client's file is empty" << std::endl;
                    header = reply(response, FILE_NOT_FOUND, request->name_len, response->filename);
                    boost::asio::write(sock, boost::asio::buffer(header));
                    return FILE_NOT_FOUND;
                }

                response->status = retrieveFileFromBackup(sock, request, response);

                if (response->status != GET_FILE_SUC) {
                    header = reply(response, response->status, request->name_len, response->filename);
                    boost::asio::write(sock, boost::asio::buffer(header));
                }
                break;

            case ERASE_FILE:
                std::cout << "Erasing file from backup: " << request->filename << std::endl;

                path.append(std::to_string(request->uid));

                // check if client's directory actually exists
                if (!boost::filesystem::exists(path) && !boost::filesystem::is_directory(path)) {
                    std::cout << "Error opening client's directory" << std::endl;
                    header = reply(response, NO_CONTENT);
                    boost::asio::write(sock, boost::asio::buffer(header));
                    return NO_CONTENT;
                }

                path.append(request->filename);

                // check if the required file exists
                if (!boost::filesystem::exists(path)) {
                    std::cout << "Client's file does not exist" << std::endl;
                    header = reply(response, FILE_NOT_FOUND, request->name_len, response->filename);
                    boost::asio::write(sock, boost::asio::buffer(header));
                    return FILE_NOT_FOUND;
                }

                response->status = eraseFile(path);
                header = reply(response, response->status, request->name_len, response->filename);
                boost::asio::write(sock, boost::asio::buffer(header));
                break;

            case GET_BACKUP_LIST:
                std::cout << "Getting list of backed up for : " << request->uid << std::endl;

                path.append(std::to_string(request->uid));

                // check if client's directory actually exists
                if (!boost::filesystem::exists(path) && !boost::filesystem::is_directory(path)) {
                    std::cout << "Error opening client's directory" << std::endl;
                    header = reply(response, NO_CONTENT);
                    boost::asio::write(sock, boost::asio::buffer(header));
                    return NO_CONTENT;
                }

                // get the list of files in client's directory
                fileList = getBackupList(path);

                if (fileList.empty()) {
                    std::cout << "Client's directory is empty" << std::endl;
                    header = reply(response, NO_CONTENT);
                    boost::asio::write(sock, boost::asio::buffer(header));
                    return NO_CONTENT;
                }

                fileName = generateRandomAlphaNum(32) + ".txt";

                // send the file list
                response->status = sendBackupList(sock, request, response, fileName, fileList);

                if (response->status != GET_LIST_SUC) {
                    std::cout << "Error occurred while sending list of files" << std::endl;
                    header = reply(response, INTERNAL_ERROR);
                    boost::asio::write(sock, boost::asio::buffer(header));
                }

                break;

            default:
                throw;
        }
    }
    catch (std::exception &e) {
        std::cerr << "Exception in thread, parseRequest: Requested operation " << std::to_string(request->op)
                  << "not available\n";
        response->status = INTERNAL_ERROR;
    }

    return response->status;
}

/* backup the file in the client directory */
uint16_t backupFile(tcp::socket &sock, Request *request, Response *response) {
    boost::system::error_code error;
    uint32_t byteCount = 0;
    uint8_t chunk[MAX_LENGTH] = {0};
    boost::filesystem::path path(boost::filesystem::current_path());

    path.append(std::to_string(request->uid));
    path.append(request->filename);
    std::cout << "Attempting to create file at path: " << path << std::endl;

    // attempt to create the backup file (append mode)
    try {
        request->payload.open(path, std::ios::out | std::ios::binary);
        if (!request->payload)
            throw std::runtime_error("backupFile(): Can't open file");
    }
    catch (const std::exception &e) {
        std::cerr << "Exception in thread, backupFile(): " << e.what() << "\n";
        return INTERNAL_ERROR;
    }

    // add the incoming packets to the file
    try {
        while (byteCount < request->size) {
            response->size = sock.read_some(boost::asio::buffer(chunk), error);
            byteCount += response->size;

            if ((error == boost::asio::error::eof) || (response->size == 0)) {
                std::cout << "EOF"
                          << "\n";
                break;
            }
            else if (error)
                throw boost::system::system_error(error); // Some other error.

            // write/append the chunk to the created file
            request->payload.write((char *) &chunk, response->size);

            // clear the data buffer before the next read
            clear_buffer(chunk, MAX_LENGTH);
        }

        if (byteCount != request->size) {
            std::cout << "Mismatch. Number of received file bytes != file size" << std::endl;
            throw boost::system::system_error(error);
        }

        std::cout << "Read " << byteCount << " bytes" << std::endl;
    }
    catch (std::exception &e) {
        std::cerr << "Exception in thread, backupFile: " << e.what() << "\n";
        request->payload.close();
        return INTERNAL_ERROR;
    }

    request->payload.close();
    return BACKUP_FILE_SUC;
}

/*
* Send back the file that the client has specified.
*/
uint16_t retrieveFileFromBackup(tcp::socket &sock, Request *request, Response *response) {
    boost::system::error_code error;
    boost::filesystem::path path(boost::filesystem::current_path());

    uint32_t size = 0;
    uint32_t byteCount = 0;
    uint8_t chunk[MAX_LENGTH] = {0};
    std::vector<uint8_t> header;

    std::cout << "Retrieving file: " << request->filename << std::endl;


    // attempt to open the file
    boost::filesystem::ifstream file;
    try {
        if (boost::filesystem::exists(std::to_string(request->uid))) {
            if ((boost::filesystem::is_directory(std::to_string(request->uid)))) {
                path.append(std::to_string(request->uid));
                if (!boost::filesystem::exists(path.append(request->filename)))
                    throw std::runtime_error("File " + request->filename + " not exist!");
            }
            else
                throw std::runtime_error("uid " + std::to_string(request->uid) + "isn't a directory!");
        }
        else {
            throw std::runtime_error("uid not exist.");
        }

        file.open(path, std::ios::out | std::ios::binary);
        if (!file)
            throw std::runtime_error("Can't open " + request->filename);
    }
    catch (const std::exception &e) {
        std::cerr << "Exception in thread, retrieveFileFromBackup: " << e.what() << "\n";
        return FILE_NOT_FOUND;
    }

    /* first, send the header of the response. then send the payload of the file */
    header = reply(response, GET_FILE_SUC,
                   request->name_len,
                   request->filename,
                   boost::filesystem::file_size(path));

    try {
        // send the response header
        boost::asio::write(sock, boost::asio::buffer(header));

        // send the file payload
        while (byteCount < boost::filesystem::file_size(path)) {
            if (file.eof()) // check if file pointer reached the end of the file
                break;

            file.read((char *) &chunk, MAX_LENGTH); // try to read MAX_LANGTH amount of bytes
            size = file.gcount();                  // get amount of bytes that were read successfuly
            byteCount += size;

            // send the chunk to client
            boost::asio::write(sock, boost::asio::buffer(chunk));

            if (error)
                throw boost::system::system_error(error); // Some other error.

            // clear the data buffer before the next read
            clear_buffer(chunk, MAX_LENGTH);
        }

        std::cout << "Sent " << byteCount << " bytes" << std::endl;
    }
    catch (std::exception &e) {
        std::cerr << "Exception in thread, retrieveFileFromBackup: " << e.what() << "\n";
        file.close();
        return INTERNAL_ERROR;
    }

    file.close();
    return GET_FILE_SUC;
}

/* erase the given file from the client's backup directory */
uint16_t eraseFile(boost::filesystem::path path) {
    try {
        boost::filesystem::remove(path);
    }
    catch (const std::exception &e) {
        std::cerr << "Exception in thread, eraseFile: " << e.what() << "\n";
        return INTERNAL_ERROR;
    }
    return ERASE_FILE_SUC;
}

/* returns a vector containing filenames. */
std::vector<std::string> getBackupList(boost::filesystem::path path) {
    std::vector<std::string> listOfFiles;

    try {
        std::cout << std::endl;

        for (const auto &entry: boost::filesystem::directory_iterator(path)) {
            std::cout << entry.path().filename().string() << std::endl;
            listOfFiles.push_back(entry.path().filename().string());
        }

        std::cout << std::endl;
    }
    catch (const std::exception &e) {
        std::cerr << "Exception in thread, getBackupList: " << e.what() << "\n";
    }
    return listOfFiles;
}

/* send the list of file in the client's backup directory.*/
uint16_t sendBackupList(tcp::socket &sock, Request *request, Response *response, std::string fileName,
                        std::vector<std::string> fileList) {
    boost::system::error_code error;
    std::vector<uint8_t> header;
    uint32_t byteCount = 0;

    std::cout << "Sending dir list file:             " << fileName << std::endl;

    /* first, send the header of the response. then send the payload of the file */
    header = reply(response,
                   GET_LIST_SUC,
                   36, // 32 + .txt
                   fileName,
                   fileList.size());
    try {
        // send the response header
        boost::asio::write(sock, boost::asio::buffer(header));

        // send the list of files
        for (auto line = fileList.begin(); line != fileList.end(); ++line) {

            /* we're appending new line to the payload so the client could parse it
               correctly */
            boost::asio::write(sock, boost::asio::buffer(*line + '\n'));

            byteCount += (*line).size();
            if (error)
                throw boost::system::system_error(error);
        }

        std::cout << "Sent " << byteCount << " bytes" << std::endl;
    }
    catch (std::exception &e) {
        std::cerr << "Exception in thread, sendBackupList(): " << e.what() << "\n";
        return INTERNAL_ERROR;
    }
    return GET_LIST_SUC;
}

std::string generateRandomAlphaNum(const int len) {
    std::string s;
    std::string chars =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";

    s.reserve(len);
    boost::random::random_device rng;
    boost::random::uniform_int_distribution<> index_dist(0, chars.size() - 1);

    for (int i = 0; i < len; i++) {
        s += chars[index_dist(rng)];
    }
    return s;
}


/* 
 * build the server's response according to the given parameter.
 * the header consist of :
 *      version    1 byte
 *      status     2 bytes
 *      name_len   2 bytes
 *      filename   variable number of bytes
 *      size       4 bytes
 *      payload    variable number of bytes
 * */
std::vector<uint8_t> reply(Response *response,
                           uint16_t status,
                           uint16_t nameLen,
                           std::string filename,
                           uint32_t size) {

    response->version = SERVER_VERSION;
    response->status = status;
    response->nameLen = nameLen;
    response->filename = filename;
    response->size = size;

    std::vector<uint8_t> header;
    header.push_back(SERVER_VERSION);
    header.push_back((uint8_t) status);
    header.push_back((uint8_t) (status >> 8));
    header.push_back((uint8_t) nameLen);
    header.push_back((uint8_t) (nameLen >> 8));

    if (!filename.empty()) {
        for (int i = 0; i < nameLen; i++)
            header.push_back(filename[i]);
    }
    if (size) {
        header.push_back((uint8_t) (size));
        header.push_back((uint8_t) (size >> 8));
        header.push_back((uint8_t) (size >> 16));
        header.push_back((uint8_t) (size >> 24));
    }
    return header;
}

/* creates a new directory with name same as client uid*/
bool mkdir(uint32_t uid) {
    boost::filesystem::path path(boost::filesystem::current_path());
    path.append(std::to_string(uid));

    std::cout << "path: " << path << std::endl;

    // if directory already exists, exit
    if (boost::filesystem::exists(path))
        return true;

    try {
        boost::filesystem::create_directory(path);
        return true;
    }
    catch (std::exception &e) {
        std::cerr << "Exception in thread, mkdir: " << e.what() << "\n";
        return false;
    }
}

/* checks if a given client id exists in the given list */
bool isClient(std::list<uint32_t> list, uint32_t id) {
    for (std::list<uint32_t>::iterator it = list.begin(); it != list.end(); ++it) {
        if (*it == id)
            return true;
    }
    return false;
}

/* listen for incoming connections and creates new thread for each new connection */
void server(boost::asio::io_context &io_context, unsigned short port) {
    tcp::acceptor a(io_context, tcp::endpoint(tcp::v4(), port));
    for (;;) {
        std::cout << "\n\n"
                  << "Waiting to accept client connection"
                  << "\n";

        std::thread(session, a.accept()).detach();

        std::cout << "Accepted "
                  << "\n\n";
    }
}

/* clears the buffer */
void clear_buffer(uint8_t *buf, uint32_t length) {
    for (uint32_t i = 0; i < length; i++)
        buf[i] = 0;
}

int main(int argc, char *argv[]) {
    boost::filesystem::ifstream file;
    boost::filesystem::path path(boost::filesystem::current_path());
    const std::string port = "port.info";
    std::string portNum = "";
    // Try and search for the port.info file
    try {
        if (!boost::filesystem::exists(port))
            throw std::runtime_error("port.info doesn't exist.");
        path.append(port);
        file.open(path, std::ios::out);
        if (!file)
            throw std::runtime_error("Can't open file" + port);
        boost::asio::io_context io_context;
        getline(file, portNum);
        std::cout << "Starting Backup Server" << std::endl;
        server(io_context, std::stoi(portNum));
    } catch (std::exception &e) {
        std::cerr << "Exception in main: " << e.what() << "\n";
    }
    return 0;
}
