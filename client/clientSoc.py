from time import sleep
import socket
import random
import os

# possible opcodes
BACKUP_FILE = 100
GET_FILE = 200
DELETE_FILE = 201
GET_BACKUP_LIST = 202

# Return codes
return_codes = {'GET_FILE_SUC': 210,
                'GET_LIST_SUC': 211,
                'BACKUP_OR_DEL_FILE_SUC': 212,
                'FILE_NOT_FOUND': 1001,
                'NO_CONTENT': 1002,
                'INTERNAL_ERROR': 1003}

# The max buffer size
buffer_size = 1024


def reply_status(code: int):
    """ returns the status code that we got from the server """
    for k, v in return_codes.items():
        if v == code:
            return k


class clientSoc:
    def __init__(self, uid=None, sock=None):
        if sock is None:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        else:
            self.sock = sock

        # create unique uid if none was given
        if uid is None:
            self.uid = random.randrange(0, 2 ** 32)
        else:
            self.uid = uid
        print(f'\nClient id: {self.uid}')

        self.server_addr = ''
        self.server_port = 0
        self.get_server_info()

        self.backup_list = self.get_local_files()

        # server's response header will be saved here
        self.version = 1
        self.status = 0  # status code from server
        self.name_len = ''  # 2 bytes
        self.filename = ''  # without null termination
        # payload fields
        self.size = 0  # 4 bytes
        self.payload = None

    def get_server_info(self):
        """
        retrieves server ip and port from the server.info file
        """
        fd = open('server.info', 'r')
        info = fd.readline().split(':')
        fd.close()

        if info[0] != '127.0.0.1':
            print('Error in server.info file. Server address isn\'t 127.0.0.1 ')
            return

        elif not (info[1].isdecimal()) or not (0 < len(info[1]) < 5):
            print('Error in server.info file. Server port isn\'t in valid format.')
            return

        self.server_addr = info[0]
        self.server_port = int(info[1])
        print(f'Server info {self.server_addr}:{self.server_port}')

    def get_local_files(self) -> list:
        """
        returns a list of filenames from the backup.info file
        """
        filenames = []
        fd = open('backup.info', 'r')
        while True:
            line = fd.readline()
            if line:
                filenames.append(line.strip())
            else:
                break
        fd.close()
        return filenames

    def connect(self, host, port):
        print(f"Connecting to server {host}:{port}")
        self.sock.connect((host, port))

    def close(self):
        print("Closing connection")
        self.sock.close()

    def response_parser(self):
        """
        parser for the responses from the server
        """

        response = self.sock.recv(buffer_size)

        self.version = response[0]
        self.status = int.from_bytes(response[1: 3], 'little')

        # because we don't always get the same responses, we'll need to parse it differently
        # case 1: name_len is included in the response
        if len(response) > 4:
            self.name_len = int.from_bytes(response[3: 5], 'little')

        # case 2 : filename and size available, parse them
        if len(response) > 6:
            self.filename = response[5: 5 + self.name_len]
            self.size = int.from_bytes(response[5 + self.name_len: 9 + self.name_len], 'little')

        print(f"Server's response code: {reply_status(self.status)}")

    def header(self, operation=0, name_len=0, filename="") -> bytes:
        """
        Constructs the client header.
        :param operation: opcodes - 1 byte
        :param name_len: filename length , 2 bytes

        """
        if operation == 0:
            raise ValueError('Operation is illegal.')

        h = self.uid.to_bytes(4, 'little')
        h += self.version.to_bytes(1, 'little')
        h += operation.to_bytes(1, 'little')
        h += name_len.to_bytes(2, 'little')
        h += filename.encode('utf-8')
        return h

    def send_file(self, fd):
        """
        reads file content in chunks of 1024 bytes and sends them to the server.
        """
        print('Uploading file..')
        chunk = True

        while chunk:
            chunk = fd.read(buffer_size)
            if chunk:
                try:
                    self.sock.sendall(chunk)
                except socket.error as e:
                    print(f'Socket connection error : {e} .')
                    return
        print('File uploaded successfully.')

    def recieve_file(self, fd, file_size: int):
        """
        retrieves chunks of the file from server
        and writes them to the file descriptor.
        :param file_size: the total file size wer'e going to download.
        :param fd: file descriptor
        :return: the number of bytes received.
        """
        print(f'Downloading file in size of: {file_size} bytes')
        size = 0

        while size <= file_size:
            try:
                chunk = self.sock.recv(buffer_size)
                size += len(chunk)

                if size <= file_size:
                    fd.write(chunk)

                else:
                    # we're getting the last chunk, so we make sure to write
                    # the necessary bytes that doesn't exceed file_size.
                    fd.write(chunk[0: len(chunk) - (size - file_size)])
                    size -= (size - file_size)
                    break

            except socket.error as e:
                print(f'Socket connection error : {e} ')
                return
        print('File downloaded successfully.')
        return size

    def backup_file(self, filename: str):
        """
        backup file request
        :param filename: the file to backup
        :return:
        """
        print(f"Backing up: {filename}")

        try:
            fd = open(filename, 'rb')
        except Exception as exc:
            print(f'backup_file(): Error while opening {filename}, {exc}')
            return

        # make sure the file isn't empty
        try:
            # read a chunk of 1024bytes
            chunk = fd.read(buffer_size)
            if not chunk:
                raise RuntimeError
        except RuntimeError:
            print(f'File {filename} is empty. Terminating backup operation')
            fd.close()
            return

        fd.seek(0)  # move file descriptor to the begining of the file

        file_size = os.stat(filename).st_size

        if file_size >= (2 ** 32):
            print(f"File size is to large: {file_size} bytes. Aborting request")
            fd.close()
            return
        print(f"Sending file in size of: {file_size} bytes")

        # construct header
        msg_header = self.header(BACKUP_FILE, name_len=len(filename), filename=filename)
        self.connect(self.server_addr, self.server_port)
        self.sock.sendall(msg_header + file_size.to_bytes(4, 'little'))

        sleep(0.1)  # adding a delay to prevent the merge of header and payload

        self.send_file(fd)
        fd.close()
        self.response_parser()

        # if the server responded that the backup failed
        if self.status != return_codes['BACKUP_OR_DEL_FILE_SUC']:
            print(f'Error while backing up : {reply_status(self.status)}')
        self.sock.shutdown(socket.SHUT_WR)
        self.close()

    def get_file(self, filename: str, save_as: str):
        """
        get file request
        :param filename: the filename we want to download
        :param save_as: the name we want to save the downloaded file.
        :return:
        """
        print(f"Requesting: {filename} from the server..")

        try:
            fd = open(save_as, 'wb')
        except Exception as exc:
            print(f'Cannot open {save_as}, {exc}')
            fd.close()
            return

        print(f'The downloaded file be saved as: {save_as}')

        # construct header
        msg_header = self.header(GET_FILE, name_len=len(filename), filename=filename)

        self.connect(self.server_addr, self.server_port)
        self.sock.sendall(msg_header)
        self.response_parser()

        # if the server can't get us the file, print the status code
        if self.status != return_codes['GET_FILE_SUC']:
            print(f'Can\'t get the file. {reply_status(self.status)}')
            # os.remove(os.path.abspath(fd.name))
            fd.close()
            self.sock.shutdown(socket.SHUT_WR)
            self.close()
            return

        sleep(0.1)  # adding a delay to prevent the merge of header and payload

        recv_size = self.recieve_file(fd, self.size)
        fd.close()
        print(f'Received file in size of: {recv_size} bytes')
        try:
            if recv_size != os.stat(filename).st_size:
                raise RuntimeError
        except RuntimeError:
            print('Warning: Mismatch. The received file size does not equal to the file size on server.')
        self.sock.shutdown(socket.SHUT_WR)
        self.close()

    def delete_file(self, filename: str):
        """
        erase file request
        :param filename: the filename we want to erase in client's backup directory
        """
        print(f"Requesting to erase : {filename}")

        msg_header = self.header(DELETE_FILE, name_len=len(filename), filename=filename)

        self.connect(self.server_addr, self.server_port)
        self.sock.sendall(msg_header)
        self.response_parser()

        # if the server can't get us the file, print the status code
        if self.status != return_codes['BACKUP_OR_DEL_FILE_SUC']:
            print(f'Received error status {reply_status(self.status)}')
        self.sock.shutdown(socket.SHUT_WR)
        self.close()

    def get_backup_list(self):
        """
        get a list of all backed up files
        """

        print("Requesting list of backed up files..")

        msg_header = self.header(GET_BACKUP_LIST)

        self.connect(self.server_addr, self.server_port)
        self.sock.sendall(msg_header)
        self.response_parser()

        # if the server can't get us the list.
        if self.status != return_codes['GET_LIST_SUC']:
            print(f'Error while trying to get the list of files. {reply_status(self.status)}')
            self.sock.shutdown(socket.SHUT_WR)
            self.close()
            return

        sleep(0.1)

        print(f'Receiving the list of backed up files, name={self.filename}, size={self.size}:')
        line = b"_"
        count = 0  # line count

        try:
            while line and (count <= self.size):
                line = self.sock.recv(buffer_size)
                count += 1
                print(line.decode())

        except socket.error as e:
            print(f'Socket error while trying to get the list of backed up files, {e}')
        finally:
            self.sock.shutdown(socket.SHUT_WR)
            self.close()
