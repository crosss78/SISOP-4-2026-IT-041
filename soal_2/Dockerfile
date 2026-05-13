FROM ubuntu:latest

RUN apt update && apt install -y \
    fuse \
    libfuse-dev \
    gcc \
    make

WORKDIR /app

COPY . /app

RUN chmod +x server
RUN mkdir -p /app/db

EXPOSE 9000

CMD ["./server"]