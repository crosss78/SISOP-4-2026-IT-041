FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    samba smbclient rsyslog bash coreutils procps \
 && rm -rf /var/lib/apt/lists/*

RUN mkdir -p /libraryit/ebooks /libraryit/papers /libraryit/sourcecode /libraryit/docs \
    /logs /run/samba

COPY smb.conf /etc/samba/smb.conf
COPY entrypoint.sh /entrypoint.sh

RUN chmod +x /entrypoint.sh

EXPOSE 137/udp 138/udp 139 445

ENTRYPOINT ["/entrypoint.sh"]