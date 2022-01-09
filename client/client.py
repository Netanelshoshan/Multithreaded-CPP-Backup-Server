from clientSoc import clientSoc


def main():
    """
    Demonstration of available options:
    1. get the list of backed up files.
    2. backup the first file that's in backup.info file.
    3. backup the second file.
    4. get the list of backed up files.
    5. download the first file from backup and save it as tmp.
    6. delete the first file from backup.
    7. get the first file from backup.

    THIS IS A FOR DEMONSTRATION ONLY.
    In the real world, the client will have an actual interface.
    After each request, the client will close the connection because we're
    working in stateless protocol.
    """
    client = clientSoc(1111)
    client.get_backup_list()

    client = clientSoc(1111)
    client.backup_file(client.backup_list[0])

    client = clientSoc(1111)
    client.backup_file(client.backup_list[1])

    client = clientSoc(1111)
    client.get_backup_list()

    client = clientSoc(1111)
    client.get_file(client.backup_list[0], 'tmp')

    client = clientSoc(1111)
    client.delete_file(client.backup_list[0])

    client = clientSoc(1111)
    client.get_file(client.backup_list[0], 'tmp2')


if __name__ == "__main__":
    main()
